#!/bin/bash

set -e

pushd build/indi-3rdparty-libs

if [[ -f build.ninja ]]; then
    $(command -v sudo) cmake --install .
else
    $(command -v sudo) make install
fi
popd

# Fix macOS library paths and ad-hoc signatures of the pre-built SDK libraries
if [[ "$(uname -s)" == "Darwin" ]]; then
    ARCH=$(uname -m)
    if [[ "$ARCH" == "arm64" ]]; then
        HOMEBREW_PREFIX="/opt/homebrew"
    else
        HOMEBREW_PREFIX="/usr/local"
    fi

    LIBUSB_PATH="$HOMEBREW_PREFIX/opt/libusb/lib/libusb-1.0.0.dylib"
    if [[ ! -f "$LIBUSB_PATH" ]]; then
        LIBUSB_PATH="$HOMEBREW_PREFIX/lib/libusb-1.0.0.dylib"
    fi

    # The SDK libraries reference @rpath/libusb-1.0.0.dylib; they can only resolve it
    # if the loader has a matching rpath (usually provided by the driver binaries),
    # or if we rewrite the link to the absolute Homebrew path.
    for lib in /usr/local/lib/libASICamera2*.dylib \
               /usr/local/lib/libEFWFilter*.dylib \
               /usr/local/lib/libEAFFocuser*.dylib \
               /usr/local/lib/libCAARotator*.dylib \
               /usr/local/lib/libUSB2ST4Conv*.dylib; do
        if [[ -f "$lib" ]]; then
            $(command -v sudo) install_name_tool -add_rpath "$HOMEBREW_PREFIX/lib" "$lib" 2>/dev/null || true
            # Re-sign: macOS requires a valid signature after rewriting load commands
            $(command -v sudo) codesign --force --sign - "$lib"
        fi
    done
fi
