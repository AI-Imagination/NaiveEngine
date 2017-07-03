#include <string>

#include "glog/logging.h"
#include "gtest/gtest.h"

#include "engine.h"

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

class MultiThreadEnginePooledTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    EngineProperty prop;
    prop.num_cpu_threads = 2;
    engine = CreateEngine("MultiThreadEnginePooled", prop);
    DLOG(INFO) << "create vars";
    // create vars
    vars["a"] = engine->NewResource();
    vars["b"] = engine->NewResource();
    vars["c"] = engine->NewResource();
    vars["d"] = engine->NewResource();
    DLOG(INFO) << "finish creating vars";
  }

  std::shared_ptr<Engine> engine;
  std::map<string, ResourceHandle> vars;
};

TEST_F(MultiThreadEnginePooledTester, NewOperation) {
  Engine::AsyncFn fn = [](RunContext ctx, CallbackOnComplete cb) {
    LOG(INFO) << "async fn run";
    cb();
  };
  auto read_vars = std::vector<ResourceHandle>{vars["a"], vars["b"]};
  auto write_vars = std::vector<ResourceHandle>{vars["c"]};
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
  auto read_vars = std::vector<ResourceHandle>{vars["a"], vars["b"]};
  auto write_vars = std::vector<ResourceHandle>{vars["c"]};
  auto opr = engine->NewOperation(fn, read_vars, write_vars);
  RunContext ctx;
  LOG(INFO) << "PushAsync";
  engine->PushAsync(opr, ctx);
  LOG(INFO) << "WaitForAllFinished";
  engine->WaitForAllFinished();
  ASSERT_TRUE(flag);
}
