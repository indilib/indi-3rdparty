#!/bin/bash

set -x -e

FLAGS="-DCMAKE_INSTALL_PREFIX=/usr/local -DFIX_WARNINGS=ON -DCMAKE_BUILD_TYPE=$1"
FLAGS+=" -DINDI_BUILD_UNITTESTS=ON"

# Build everything on master
echo "==> Building INDI 3rd party drivers"
mkdir -p build/3rdparty
pushd build/3rdparty
cmake $FLAGS . ../../3rdparty/
make
make install
cmake $FLAGS . ../../3rdparty/
make
popd

exit 0
