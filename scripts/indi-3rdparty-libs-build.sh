#!/bin/bash

set -e

SRCS=$(dirname $(realpath $0))/..

mkdir -p build/indi-3rdparty-libs
pushd build/indi-3rdparty-libs
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DFIX_WARNINGS=ON \
    -DCMAKE_BUILD_TYPE=$1 \
    -DINDI_BUILD_UNITTESTS=ON \
    -DBUILD_LIBS=1 \
    . $SRCS

make -j3
popd
