#!/bin/bash

set -x -e

FLAGS="-DCMAKE_INSTALL_PREFIX=/usr/ -DFIX_WARNINGS=ON -DCMAKE_BUILD_TYPE=$1"
FLAGS+=" -DINDI_BUILD_UNITTESTS=ON"

# Build everything on master
echo "==> Building INDI 3rd party drivers"
mkdir -p build
pushd build
cmake $FLAGS . ../
make
make install
cmake $FLAGS . ../
make
popd

exit 0
