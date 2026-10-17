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

SRCS=$(dirname $(realpath $0))/..

if [[ "$(uname -s)" == "Darwin" ]]; then
    # Detect architecture
    ARCH=$(uname -m)
    if [[ "$ARCH" == "arm64" ]]; then
        HOMEBREW_PREFIX="/opt/homebrew"
        OSX_ARCH="arm64"
    else
        HOMEBREW_PREFIX="/usr/local"
        OSX_ARCH="x86_64"
    fi

    # Use Homebrew LLVM clang (Apple's default clang may miss C++ headers)
    CLANG_C="$HOMEBREW_PREFIX/opt/llvm/bin/clang"
    CLANG_CXX="$HOMEBREW_PREFIX/opt/llvm/bin/clang++"

    if [[ -x "$CLANG_CXX" ]]; then
        CMAKE_COMPILER_FLAGS=(
            -DCMAKE_C_COMPILER="$CLANG_C"
            -DCMAKE_CXX_COMPILER="$CLANG_CXX"
        )
    fi

    # macOS-specific CMake flags
    CMAKE_MACOS_FLAGS=(
        -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCH"
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
        -DCMAKE_CXX_FLAGS="-Wno-error=deprecated-declarations -Wno-error=unused-but-set-variable -Wno-error=unused-but-set-global"
        -DCMAKE_C_FLAGS="-Wno-error=unused-but-set-variable -Wno-error=unused-but-set-global"
        -DINDI_INSTALL_FIRMWARE=OFF
        -DINDI_INSTALL_UDEV_RULES=OFF
        -DFIX_MACOS_LIBS=ON
    )
fi

mkdir -p build/indi-3rdparty-libs
pushd build/indi-3rdparty-libs
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DFIX_WARNINGS=ON \
    -DCMAKE_BUILD_TYPE=$1 \
    -DINDI_BUILD_UNITTESTS=ON \
    -DBUILD_LIBS=1 \
    "${CMAKE_COMPILER_FLAGS[@]}" \
    "${CMAKE_MACOS_FLAGS[@]}" \
    . $SRCS

make -j$(($(nproc)+1))
popd
