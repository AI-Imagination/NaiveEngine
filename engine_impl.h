#pragma once

#include "engine.h"

namespace engine {

struct Resource {
  template <typename T> inline T *Cast() {
    static_assert(std::is_base_of<Resource, T>::value,
                  "should be inherinted from Resource");
    return static_cast<T *>(this);
  }
};

struct Operation {
  template <typename T> inline T *Cast() {
    static_assert(std::is_base_of<Operation, T>::value,
                  "should be inherinted from Operation");
    return static_cast<T *>(this);
  }
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
               const std::vector<ResourceHandle> &write_res) override {
    DLOG(INFO) << "DebugEngine new operation";
    return std::make_shared<DebugOpr>(fn);
  }

  // Create a new Resource.
  virtual ResourceHandle NewResource() override { return nullptr; }

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

} // namespace engine
