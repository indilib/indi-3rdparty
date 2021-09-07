#!/bin/bash

set -e

pushd build/indi-3rdparty-libs
$(command -v sudo) make install
popd
