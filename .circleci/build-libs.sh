#!/bin/bash

# This is a script building libraries for circle-ci
# It is *not* for general audience

SRC=../../indi-3rdparty/
FLAGS="-DCMAKE_INSTALL_PREFIX=/usr -DFIX_WARNINGS=ON -DCMAKE_BUILD_TYPE=$1"

LIBS="libapogee libfishcamp libfli libqhy libqsi libsbig libinovasdk libahp-xc"

if [ .${CIRCLE_BRANCH%_*} == '.drv' -a `lsb_release -si` == 'Ubuntu' ] ; then
    DRV=lib"${CIRCLE_BRANCH#drv_}"
    if [ -d 3rdparty/$DRV ] ; then
        LIBS="$DRV"
    else
        LIBS=""
    fi
    echo "[$DRV] [$LIBS]"
fi

for lib in $LIBS ; do
    echo "Building $lib ..."
    mkdir -p build/$lib
    pushd build/$lib
    cmake $FLAGS . $SRC/$lib
    make
    make install
    popd
done
