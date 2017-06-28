#include <string>

#include "engine.h"
#include "gtest/gtest.h"

using namespace engine;

TEST(DebugEngine, init) {
  EngineProperty prop;
  auto engine = CreateEngine("DebugEngine", prop);
}
