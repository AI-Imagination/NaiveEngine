#include <string>

#include "glog/logging.h"
#include "gtest/gtest.h"

#include "engine.h"

using namespace engine;

class DebugEngineTester : public ::testing::Test {
public:
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

