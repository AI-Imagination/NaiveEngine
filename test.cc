#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "glog/logging.h"
#include "gtest/gtest.h"

#include "engine.h"
#include "engine_impl.h"
#include "profile.h"
#include "swiftcpp/thread_utils.h"

using namespace engine;

class DebugEngineTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    EngineProperty prop;
    engine = CreateEngine("DebugEngine", prop);
  }

  std::shared_ptr<Engine> engine;
};

TEST(DebugEngine, init) {
  EngineProperty prop;
  auto engine = CreateEngine("DebugEngine", prop);
}

TEST_F(DebugEngineTester, init) {}

TEST_F(DebugEngineTester, NewOperation) {
  auto fn = [](RunContext ctx, engine::CallbackOnComplete cb) {
    LOG(INFO) << "debug async fn run.";
    cb();
  };

  auto opr = engine->NewOperation(fn, {}, {});
  ASSERT_TRUE(opr);
}

TEST_F(DebugEngineTester, PushAsync) {
  auto fn = [](RunContext ctx, engine::CallbackOnComplete cb) {
    LOG(INFO) << "debug async fn run.";
    cb();
  };
  RunContext ctx;
  engine->PushAsync(fn, ctx, {}, {});
}

TEST_F(DebugEngineTester, Terminate) { engine->Terminate(); }

TEST(task_queue, multithread) {
  int count = 0;

  swiftcpp::thread::TaskQueue<int> task_queue;
  swiftcpp::thread::ThreadPool workers(3, [&] {
    while (true) {
      int task;
      bool flag = task_queue.Pop(&task);
      if (!flag)
        return;
      count++;
      DLOG(INFO) << task;
    }
  });

  for (int i = 0; i < 100; i++) {
    task_queue.Push(i);
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));
  task_queue.SignalForKill();
}

class MultiThreadEnginePooledTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    EngineProperty prop;
    prop.num_cpu_threads = 1;
    engine = CreateEngine("MultiThreadEnginePooled", prop);

    var_a = engine->NewResource("a");
    var_b = engine->NewResource("b");
    var_c = engine->NewResource("c");
    var_d = engine->NewResource("d");
    var_e = engine->NewResource("e");
  }

  ResourceHandle var_a, var_b, var_c, var_d, var_e;

  std::shared_ptr<Engine> engine;
  std::map<string, ResourceHandle> vars;
};

TEST_F(MultiThreadEnginePooledTester, NewOperation) {
  Engine::AsyncFn fn = [](RunContext ctx, CallbackOnComplete cb) {
    LOG(INFO) << "async fn run";
    cb();
  };
  auto read_vars = std::vector<ResourceHandle>{var_a, var_b};
  auto write_vars = std::vector<ResourceHandle>{var_c};
  auto opr = engine->NewOperation(fn, read_vars, write_vars);
}

TEST_F(MultiThreadEnginePooledTester, PushAsync) {
  bool flag = false;
  Engine::AsyncFn fn = [&](RunContext ctx, CallbackOnComplete cb) {
    LOG(INFO) << "async fn run";
    flag = true;
    LOG(INFO) << "flog: " << flag;
    cb();
  };
  auto read_vars = std::vector<ResourceHandle>{var_a, var_b};
  auto write_vars = std::vector<ResourceHandle>{var_c};
  auto opr = engine->NewOperation(fn, read_vars, write_vars);
  RunContext ctx;
  LOG(INFO) << "PushAsync";
  engine->PushAsync(opr, ctx);
  LOG(INFO) << "WaitForAllFinished";
  engine->WaitForAllFinished();
  ASSERT_TRUE(flag);
}

TEST_F(MultiThreadEnginePooledTester, PushAsync_Order) {
  Engine::AsyncFn fn = [&](RunContext ctx, CallbackOnComplete cb) {
    LOG(INFO) << "async fn run";
    cb();
  };
  profile::Profiler::Get()->Clear(); // functions are
  //   func0: A = B + 1
  //   func1: C = A + 1
  //   func2: D = B + A
  //   func3: B = D
  // order:
  //   func0
  //   func1, func2
  //   func3
  auto func0 = engine->NewOperation(fn, {var_b}, {var_c}, "func0");
  auto func1 = engine->NewOperation(fn, {var_a}, {var_c}, "func1");
  auto func2 = engine->NewOperation(fn, {var_b, var_a}, {var_d}, "func2");
  auto func3 = engine->NewOperation(fn, {var_d}, {var_b}, "func3");

  engine::RunContext ctx;
  engine->PushAsync(func0, ctx);
  engine->PushAsync(func1, ctx);
  engine->PushAsync(func2, ctx);
  engine->PushAsync(func3, ctx);

#if USE_PROFILE
  DLOG(INFO) << profile::Profiler::Get()->debug_string();
#endif

  for (auto var :
       std::vector<ResourceHandle>{var_a, var_b, var_c, var_d, var_e}) {
    DLOG(INFO) << var->Cast<ThreadedResource>()->debug_string();
  }

  engine->WaitForAllFinished();
}
