#!/bin/bash
function abort(){
    echo "Your change doesn't follow spring's code style." 1>&2
    echo "Please use pre-commit to check what is wrong." 1>&2
    exit 1
}

trap 'abort' 0
set -e

cd $TRAVIS_BUILD_DIR
export PATH=/usr/bin:$PATH
pre-commit install
clang-format --version

if ! pre-commit run -a ; then
    git diff
    exit 1
fi

trap : 0
