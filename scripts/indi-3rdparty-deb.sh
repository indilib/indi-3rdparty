#!/bin/bash

set -e

command -v realpath >/dev/null 2>&1 || realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

command -v nproc >/dev/null 2>&1 || function nproc {
    command -v sysctl >/dev/null 2>&1 &&  \
        sysctl -n hw.logicalcpu ||
        echo "3"
}

INDI_SRCS=$(dirname $(realpath $0))/..

DRIVER_LIST=$@

if [ $# -eq 0 ]; then
    echo "usage: $0 drivername [...]"
    exit 1 
fi

for driver in ${DRIVER_LIST}; do
    if [ ! -d "${INDI_SRCS}/${driver}" ]; then
        echo "${INDI_SRCS}/${driver} directory doesn't exist"
        exit 1
    fi
done

for driver in ${DRIVER_LIST}; do
    rm -rf build/deb_indi-3rdparty_${driver}
    mkdir -p build/deb_indi-3rdparty_${driver}
    pushd build/deb_indi-3rdparty_${driver}
    cp -r ${INDI_SRCS}/${driver} .
    cp -r ${INDI_SRCS}/debian/${driver} debian
    cp -r ${INDI_SRCS}/cmake_modules ./
    fakeroot debian/rules -j$(($(nproc)+1)) binary
    popd
done

echo
echo "Avaliable packages:"
ls -l build/*.deb
