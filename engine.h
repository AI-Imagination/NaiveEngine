#pragma once

#include "glog/logging.h"
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace std;

namespace engine {

// Resources that can be operated on.
struct Resource;
typedef std::shared_ptr<Resource> ResourceHandle;

// Operations that can operate on resources.
struct Operation;
typedef std::shared_ptr<Operation> OperationHandle;

enum OprPriority { kNormalPriority, kHighPriority };

enum OprProperty {
  kAsync,
  kCPU_Compute,
  kGPU_Compute,
  kCPU_GPU_Copy,
  kGPU_CPU_Copy
};

struct RunContext {
  OprPriority priority;
  OprProperty property;
};

class Engine;
struct CallbackOnComplete {
  using Fn = std::function<void(OperationHandle)>;
  OperationHandle opr{nullptr};
  // Use a pointer to make the callback' reuse cheapper.
  Fn *cb{nullptr};
  Engine *engine{nullptr};

  CallbackOnComplete(OperationHandle opr, Fn *cb, Engine *engine)
      : opr(opr), cb(cb), engine(engine) {}

  void operator()() const { (*cb)(opr); }
};

class Engine {
public:
  // using CallbackOnComplete = std::function<void(Engine *, OperationHandle)>;
  using AsyncFn = std::function<void(RunContext, CallbackOnComplete)>;
  using SyncFn = std::function<void(RunContext)>;

  // Push an asynchronous task to the engine, the caller thread will
  // continue running.
  virtual void PushAsync(OperationHandle opr, RunContext ctx) = 0;
  virtual void PushAsync(AsyncFn fn, RunContext ctx,
                         const std::vector<ResourceHandle> &read_res,
                         const std::vector<ResourceHandle> &write_res) {
    auto opr = NewOperation(fn, read_res, write_res);
    PushAsync(opr, ctx);
  }

  // Push a synchronous task to the engine, the caller thread will wait until
  // the task is finished.
  virtual void PushSync(SyncFn fn, RunContext ctx,
                        const std::vector<ResourceHandle> &read_res,
                        const std::vector<ResourceHandle> &write_res) = 0;

  // Create a new operation.
  virtual OperationHandle
  NewOperation(AsyncFn fn, const std::vector<ResourceHandle> &read_res,
               const std::vector<ResourceHandle> &write_res) = 0;
  // Create a new Resource.
  virtual ResourceHandle NewResource() = 0;

  // Wait until all tasks pushed to engine are finished.
  virtual void WaitForAllFinished() = 0;

  // Wait for the resources ready to read.
  virtual void WaitForResource(const std::vector<ResourceHandle> &res) = 0;

  // Stop all worker threads' work, and terminal all tasks.
  virtual void Terminate() = 0;

  static Engine *Get();
};

struct EngineProperty {
  int num_cpu_threads{1};
  int num_threads_per_gpu_device{1};
  int num_threads_gpu_copy_per_device{1};
};

std::shared_ptr<Engine> CreateEngine(const std::string &kind,
                                     EngineProperty prop);
} // namespace engine
