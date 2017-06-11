#ifndef NAIVE_ENGINE_ENGINE_H_
#define NAIVE_ENGINE_ENGINE_H_

#include <iostream>
#include <memory>

namespace naive_engine {

// Variable, a Var is a placeholder for resource, Var determines the dependency
// relations.
struct Var;
// Operation, Opr is a placeholder for a real task pushed into engine.
struct Opr;

typedef Var *VarHandle;
typedef Opr *OprHandle;

// Operation's priority.
enum OptPriority { kNormalPriority, kHighPriority };

// Executation context.
struct Context {
  // ID of the device to execute on.
  int device_id = -1;
  // whether in GPU mode.
  bool is_gpu = false;

  OptPriority priority = kNormalPriority;

}; // struct Context

// Operation's property
enum OptProperty {
  // IO related, such as copy between CPU and GPU.
  kIO,
  // Common task.
  kAsync
};

// API for all Engine implementations.
class Engine {
public:
  struct CompleteCallbackFn;

  // A function warps operation task.
  using Fn = std::function<void(Context)>;
  // Callback function to run after an async operation is finished.
  // using CompleteCallbackFn = std::function<void()>;
  // Async Function to call asynchronously.
  using AsyncFn = std::function<void(Fn, CompleteCallbackFn)>;

  // Get a singleton.
  static Engine *Instance();

  // Push an operation into engine and return, the pusher's thread will not
  // hang.
  //
  // @fn: the operation.
  // @priority: priority of the operaion.
  virtual void Push(AsyncFn fn, Context ctx,
                    const std::vector<VarHandle> &const_vars,
                    const std::vector<VarHandle> &mutate_vars,
                    const char *name = nullptr) = 0;

  // Register a operation's read dependency on a variable `var`,  the operation
  // wait until `var` is ready to read.
  virtual void RegisterReadDependency(OprHandle opr, VarHandle var) = 0;

  // Register a operation's write dependency on a variable `var`,  the operation
  // wait until `var` is ready to write.
  virtual void RegisterWriteDependency(OprHandle opr, VarHandle var) = 0;

  // Finish writing the variable and delete the dependency.
  virtual void FinishWriteDependency(OprHandle opr, VarHandle var) = 0;

  // Finish reading the variable and delete the dependency.
  virtual void FinishReadDependency(OprHandle opr, VarHandle var) = 0;

  // Create a new variable.
  virtual VarHandle NewVariable() = 0;

  // Create a new operator.
  // @name: name of the operator.
  // @fn: AsyncFn that wrap the task.
  // @ctx: function's execution context.
  // @const_vars: variables that requires to read.
  // @mutate_vars: variables that requires to write.
  virtual OprHandle NewOperator(AsyncFn fn, Context ctx,
                                const std::vector<VarHandle> &const_vars,
                                const std::vector<VarHandle> &mutate_vars,
                                const char *name = nullptr) = 0;

  // Shutdown the engine and tell all work threads to quit.
  virtual void ShutDown() = 0;

  struct CompleteCallbackFn {
    VarHandle var;
    OprHandle opr;
    Engine *engine;
    bool is_read = true;

    void operator()() {
      if (is_read) {
        engine->FinishReadDependency(opr, var);
      } else {
        engine->FinishWriteDependency(opr, var);
      }
    }
  };

protected:
  // Create a complete callback.
  CompleteCallbackFn CreateCompleteCallback(VarHandle var, OprHandle opr,
                                            bool rw) {
    CompleteCallbackFn cb;
    cb.var = var;
    cb.opr = opr;
    cb.is_read = rw;
    cb.engine = this;
    return cb;
  }

}; // class Engine

} // namespace naive_engine
#endif
