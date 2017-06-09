set -ex

readonly WORKSPACE=$(pwd)
readonly THIRD_SOURCE=$WORKSPACE/third-party
readonly BUILD_TARGET=$WORKSPACE/third-party/local

mkdir -p $BUILD_TARGET/lib
mkdir -p $BUILD_TARGET/include

function compile_glog {
  local path=$THIRD_SOURCE/glog
  mkdir -p $path/build
  # compile
  cd $path/build
  cmake ..
  make -j2
  #install
  cp -rf *.a $BUILD_TARGET/lib
  cp -rf glog $BUILD_TARGET/include
}

function compile_gtest {
  local path=$THIRD_SOURCE/googletest
  mkdir -p $path/build
  # compile
  cd $path/build
  cmake ..
  make -j2
  # install
  cp -rf googlemock/gtest/*.a $BUILD_TARGET/lib
  cp -rf $THIRD_SOURCE/googletest/googletest/include/gtest $BUILD_TARGET/include
}

compile_glog
compile_gtest
