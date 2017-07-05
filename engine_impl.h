#pragma once

#include <deque>

#include "engine.h"

namespace engine {

struct Resource {
#ifndef NDEBUG
  Resource() {}
  virtual ~Resource() {}
#endif
  template <typename T> inline T *Cast() {
    static_assert(std::is_base_of<Resource, T>::value,
                  "should be inherinted from Resource");
#ifdef NDEBUG
    return static_cast<T *>(this);
#else
    auto ptr = dynamic_cast<T *>(this);
    CHECK(ptr);
    return ptr;
#endif
  }
};

struct Operation {
#ifndef NDEBUG
  Operation() {}
  virtual ~Operation() {}
#endif
  template <typename T> inline T *Cast() {
    static_assert(std::is_base_of<Operation, T>::value,
                  "should be inherinted from Operation");
#ifdef NDEBUG
    return static_cast<T *>(this);
#else
    auto ptr = dynamic_cast<T *>(this);
    CHECK(ptr) << "wrong operation type";
    return ptr;
#endif
  }
  std::string name;
};

// A simple engine without thread pool, just for debug.
class DebugEngine : public Engine {
public:
  DebugEngine(const std::string &name = "debug engine") : name_(name) {}

  struct DebugOpr : public Operation {
    AsyncFn fn;
    DebugOpr(const AsyncFn &fn) : fn(fn) {}
  };

  virtual void PushAsync(OperationHandle opr, RunContext ctx) override {
    auto cb = CreateCompleteCallback(opr);
    opr->Cast<DebugOpr>()->fn(ctx, cb);
  }

  virtual void
  PushAsync(AsyncFn fn, RunContext ctx,
            const std::vector<ResourceHandle> &read_res,
            const std::vector<ResourceHandle> &write_res) override {
    auto opr = NewOperation(fn, read_res, write_res);
    auto cb = CreateCompleteCallback(opr);
    fn(ctx, cb);
  }

  virtual void PushSync(SyncFn fn, RunContext ctx,
                        const std::vector<ResourceHandle> &read_res,
                        const std::vector<ResourceHandle> &write_res) override {
    fn(ctx);
  }

  virtual OperationHandle
  NewOperation(AsyncFn fn, const std::vector<ResourceHandle> &read_res,
               const std::vector<ResourceHandle> &write_res,
               const std::string &name = "") override {
    DLOG(INFO) << "DebugEngine new operation";
    return std::make_shared<DebugOpr>(fn);
  }

  // Create a new Resource.
  virtual ResourceHandle NewResource(const std::string &name = "") override {
    return nullptr;
  }

  virtual void WaitForAllFinished() override {}

  virtual void
  WaitForResource(const std::vector<ResourceHandle> &res) override {}

  virtual void Terminate() override {
    LOG(WARNING) << "DebugEngine terminated";
  }

  // Create a Callback for use.
  static CallbackOnComplete CreateCompleteCallback(OperationHandle opr) {
    static CallbackOnComplete::Fn fn = [](OperationHandle opr) {
      DLOG(INFO) << "debug callback on complete run";
    };
    return CallbackOnComplete(opr, &fn, nullptr);
  }

private:
  std::string name_;
};

struct ThreadedOperation : public Operation {
  Engine::AsyncFn fn;
  // Resources that require to read.
  std::vector<ResourceHandle> read_res;
  // Resources that require to write.
  std::vector<ResourceHandle> write_res;
  // Name for debug.
  std::string name;
  // Runing context.
  RunContext ctx;
  Engine *engine;
  // Some resources are ready.
  void TellResReady(int num = 1) {
    noready_resource_count_ -= num;
    DLOG(INFO) << "tell res ready :" << noready_resource_count_;
  }
  // Whether the operation is ready to run.
  bool ReadyToExecute() { return noready_resource_count_ == 0; }

  ThreadedOperation(Engine *engine, const Engine::AsyncFn &fn,
                    const std::vector<ResourceHandle> &read_res,
                    const std::vector<ResourceHandle> &write_res,
                    const std::string &name = "")
      : engine(engine), fn(fn), read_res(read_res), write_res(write_res),
        noready_resource_count_(read_res.size() + write_res.size()),
        name(name) {}

private:
  // Number of resources that is not ready for this operation.
  std::atomic<int> noready_resource_count_{0};
};

// A FIFO queue for a Resource, which records all the operation dependency.
class ThreadedResource : public Resource {
public:
  using Dispatcher = std::function<void(OperationHandle)>;

  ThreadedResource(const Dispatcher &dispatcher, const std::string &name = "")
      : dispatcher_(dispatcher), name_(name) {}
  // Append a read/write denpendency to the queue.
  void AppendDependency(OperationHandle opr, bool is_write);

  // Finish a read/write dependency to the queue.
  void FinishedDependency(OperationHandle opr, bool is_write);

  // Human-readable string.
  std::string debug_string() const;

protected:
  template void ProcessQueueFront();

private:
  struct ResourceBlock {
    OperationHandle operation;
    bool is_write{false};
    ResourceBlock(OperationHandle operation, bool is_write)
        : operation(operation), is_write(is_write) {}
  };

  std::deque<ResourceBlock> queue_;
  std::atomic<int> pending_read_count_{0};
  std::atomic<bool> pending_write_{false};
  std::mutex mut_;
  std::string name_;
  Dispatcher dispatcher_;
};

} // namespace engine
