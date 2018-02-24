cmake_minimum_required(VERSION 3.2)
project(NaiveEngine)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fPIC")
set(ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(THIRD_PARTY_PATH "${CMAKE_BINARY_DIR}/third_party" CACHE STRING
        "A path setting third party libraries download & build directories.")

option(WITH_TESTING     "Compile with unit testing"        ON)

include(external/gflags)     # download, build, install gflags
include(external/glog)       # download, build, install glog
include(external/gtest)      # download, build, install gtest


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(engine_test test.cc engine.cc engine_impl.h)

enable_testing ()
target_link_libraries(engine_test glog gtest pthread)
add_test(NAME engine_test COMMAND ./engine_test)
