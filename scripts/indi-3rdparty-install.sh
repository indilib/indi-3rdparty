#!/bin/bash

set -e

pushd build/indi-3rdparty
$(command -v sudo) make install
popd
