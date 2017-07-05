#include <atomic>
#include <deque>
#include <glog/logging.h>
#include <memory>
#include <sstream>
#include <type_traits>

#include "engine.h"
#include "engine_impl.h"
#include "profile.h"
#include "swiftcpp/thread_utils.h"

namespace engine {

// ----------------------------------------------------------------------------
// Multi-thread Engine
// ----------------------------------------------------------------------------
void ThreadedResource::AppendDependency(OperationHandle opr, bool is_write) {
  DLOG(INFO) << "append " << (is_write ? " write " : " read ") << " dependency";
  std::lock_guard<std::mutex> l(mut_);
  queue_.emplace_back(opr, is_write);

  ProcessQueueFront();
}

void ThreadedResource::FinishedDependency(OperationHandle opr, bool is_write) {
  std::lock_guard<std::mutex> l(mut_);
  if (is_write) {
    pending_write_ = false;
  } else {
    pending_read_count_--;
  }

  ProcessQueueFront();
}

std::string ThreadedResource::debug_string() const {
  std::stringstream ss;
  ss << "Var " << name_ << " size: " << queue_.size();
  for (auto &opr : queue_) {
    ss << "\t" << opr.operation->Cast<ThreadedOperation>()->name << " ";
  }
  return ss.str();
}

// NOTE Not thread safe.
void ThreadedResource::ProcessQueueFront() {
  if (pending_read_count_ > 0 && pending_write_)
    return;
  if (queue_.empty())
    return;

  // front is wirte operation
  if (queue_.front().is_write) {
    if (pending_read_count_ > 0) {
      return;
    }
    // write dependency is ready.
    auto opr = queue_.front().operation;
    auto topr = opr->template Cast<ThreadedOperation>();
    topr->TellResReady();
    if (topr->ReadyToExecute()) {
      // dispatch write operation
      dispatcher_(opr);
    }
    pending_write_ = true;
    queue_.pop_front();
    // read operation
  } else {
    while (!queue_.empty() && !queue_.front().is_write) {
      auto opr = queue_.front().operation->template Cast<ThreadedOperation>();
      opr->TellResReady();
      if (opr->ReadyToExecute()) {
        dispatcher_(queue_.front().operation);
      }
      queue_.pop_front();
    }
  }
}

class MultiThreadEngine : public Engine {
public:
  virtual void PushAsync(OperationHandle opr, RunContext ctx) override {
    DLOG(INFO) << "push a func " << opr->Cast<ThreadedOperation>()->name;
#if USE_PROFILE
    auto opr_name = opr->Cast<ThreadedOperation>()->name;
    profile::Profiler::Get()->AddPushAction(opr_name);
#endif
    inc_num_padding_tasks();
    auto dispatcher = [this](OperationHandle opr) {
      DLOG(INFO) << "dispatch the opr";
      this->PushToExecute(opr);
    };
    auto topr = opr->Cast<ThreadedOperation>();
    topr->ctx = ctx;
    DLOG(INFO) << "register read resources";
    for (auto res : topr->read_res) {
      CHECK(res);
      res->template Cast<ThreadedResource>()->AppendDependency(opr, false);
    }
    DLOG(INFO) << "register write resources";
    for (auto res : topr->write_res) {
      res->template Cast<ThreadedResource>()->AppendDependency(opr, true);
    }
  }

  virtual void PushSync(SyncFn fn, RunContext ctx,
                        const std::vector<ResourceHandle> &read_res,
                        const std::vector<ResourceHandle> &write_res) override {
    AsyncFn afn = [&](RunContext ctx, CallbackOnComplete cb) { fn(ctx); };
    auto opr = NewOperation(afn, read_res, write_res);
    PushAsync(opr, ctx);
  }

  virtual OperationHandle
  NewOperation(AsyncFn fn, const std::vector<ResourceHandle> &read_res,
               const std::vector<ResourceHandle> &write_res,
               const std::string &name = "") override {
    auto opr = std::make_shared<ThreadedOperation>(this, fn, read_res,
                                                   write_res, name);
    CHECK_EQ(opr->Cast<ThreadedOperation>()->engine, (void *)this);
    CHECK_EQ((void *)this, this);
    return opr;
  }

  virtual void WaitForAllFinished() override {
    std::unique_lock<std::mutex> l(mut_);
    finish_cond_.wait(
        l, [this]() { return num_pending_tasks_ == 0 || terminated_; });
    DLOG(INFO) << "WaitForAllFinished done";
  }

  virtual void
  WaitForResource(const std::vector<ResourceHandle> &res) override {
    AsyncFn fn;
    // TODO
  }

  // Push the opr to device and execute it.
  virtual void PushToExecute(OperationHandle opr) = 0;

  static CallbackOnComplete CreateCompleteCallback(const void *engine,
                                                   OperationHandle opr) {
    DLOG(INFO) << "create Callback engine " << engine;
    CHECK_EQ(opr->Cast<ThreadedOperation>()->engine, engine);
    static CallbackOnComplete::Fn fn = [](OperationHandle opr) {
      auto engine = opr->Cast<ThreadedOperation>()->engine;
      auto ptr = opr->Cast<ThreadedOperation>();
      for (const auto &var : ptr->read_res) {
        var->Cast<ThreadedResource>()->FinishedDependency(opr, false);
      }
      for (const auto &var : ptr->write_res) {
        var->Cast<ThreadedResource>()->FinishedDependency(opr, true);
      }
      auto engine_ptr = static_cast<MultiThreadEngine *>(engine);
      engine_ptr->dec_num_padding_tasks();
#if USE_PROFILE
      profile::Profiler::Get()->AddFinishAction(
          opr->Cast<ThreadedOperation>()->name);
#endif
    };
    return CallbackOnComplete(opr, &fn, (void *)engine);
  }

protected:
  void inc_num_padding_tasks() {
    int i = num_pending_tasks_;
    num_pending_tasks_++;
    DLOG(INFO) << "engine: " << this << " inc pedding tasks " << i << " "
               << num_pending_tasks_;
  }
  void dec_num_padding_tasks() {
    int i = num_pending_tasks_;
    num_pending_tasks_--;
    DLOG(INFO) << "engine: " << this << " dec pedding tasks " << i << " "
               << num_pending_tasks_;

    finish_cond_.notify_all();
  }
  // NOTE should be updated by Terminate method.
  // Whether the engine is terminated.
  std::atomic<bool> terminated_{false};
  // number of tasks in engine.
  // NOTE should be updated in CallbackOnComplted.
  std::atomic<int> num_pending_tasks_{0};
  // Condition variable used to determine whether all the tasks are finished.
  std::condition_variable finish_cond_;
  std::atomic<uint64_t> sync_counter_;
  std::mutex mut_;
};

// ----------------------------------------------------------------------------
// Multi-thread Engine With Thread Pools
// ----------------------------------------------------------------------------
template <typename OprType> struct ThreadQueueBlock {
  ThreadQueueBlock(int n_threads)
      : workers(n_threads, [this] {
          OperationHandle o;
          while (true) {
            bool suc = task_queue.Pop(&o);
            if (!suc) {
              DLOG(WARNING)
                  << "thread " << std::this_thread::get_id() << " stop ...";
              return;
            }
            // DLOG(INFO) << "queue.size: " << task_queue.Size();
            auto ptr = o->Cast<OprType>();
            auto engine = ptr->engine;
            auto complete_callback =
                MultiThreadEngine::CreateCompleteCallback(engine, o);
            CHECK_EQ(complete_callback.engine, engine);
            CHECK(ptr->fn);
            DLOG(INFO) << "task " << ptr->name << " execute";
            ptr->fn(ptr->ctx, complete_callback);
          }
        }) {}

  swiftcpp::thread::TaskQueue<OperationHandle> task_queue;
  swiftcpp::thread::ThreadPool workers;
};

class MultiThreadEnginePooled final : public MultiThreadEngine {
public:
  MultiThreadEnginePooled(int n_common_threads = 10, int n_io_threads = 1)
      : common_task_workers_(n_common_threads), io_task_workers_(n_io_threads) {
  }
  ~MultiThreadEnginePooled() { Terminate(); }
  virtual ResourceHandle NewResource(const std::string &name = "") override;
  virtual void PushToExecute(OperationHandle opr) override;
  virtual void Terminate() override;

private:
  ThreadQueueBlock<ThreadedOperation> common_task_workers_;
  ThreadQueueBlock<ThreadedOperation> io_task_workers_;
};

ResourceHandle MultiThreadEnginePooled::NewResource(const std::string &name) {
  ThreadedResource::Dispatcher dispatcher = [this](OperationHandle opr) {
    PushToExecute(opr);
  };
  return std::make_shared<ThreadedResource>(dispatcher, name);
}

void MultiThreadEnginePooled::Terminate() {
  if (!terminated_) {
    DLOG(WARNING) << "MultiThreadEnginePooled terminated.";
    terminated_ = true;
    common_task_workers_.task_queue.SignalForKill();
    io_task_workers_.task_queue.SignalForKill();
  }
}

void MultiThreadEnginePooled::PushToExecute(OperationHandle opr) {
  auto ptr = opr->Cast<ThreadedOperation>();
  DLOG(INFO) << "run task " << opr->Cast<ThreadedOperation>()->name;
  DLOG(INFO) << "queue size " << common_task_workers_.task_queue.Size();
  if (ptr->ctx.property == kCPU_GPU_Copy ||
      ptr->ctx.property == kGPU_CPU_Copy) {
    io_task_workers_.task_queue.Push(opr);
  } else {
    common_task_workers_.task_queue.Push(opr);
  }
}

std::shared_ptr<Engine> CreateEngine(const std::string &kind,
                                     EngineProperty prop) {
  if (kind == "DebugEngine") {
    return std::make_shared<DebugEngine>();
  } else if (kind == "MultiThreadEnginePooled") {
    return std::make_shared<MultiThreadEnginePooled>(
        prop.num_cpu_threads, prop.num_threads_gpu_copy_per_device);
  }
  return nullptr;
}

}; // namespace engine
