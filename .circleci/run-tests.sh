#!/bin/bash

if [ .${TRAVIS_BRANCH%_*} == '.drv' ] ; then
    exit 0
else
    cd build/indi-3rdparty/test
    ctest -V
fi
