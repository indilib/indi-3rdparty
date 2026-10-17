#!/bin/bash

set -e

pushd build/indi-3rdparty
if [[ -f build.ninja ]]; then
    $(command -v sudo) cmake --install .
else
    $(command -v sudo) make install
fi
popd

# Re-sign macOS binaries after installation (ad-hoc signature, required after
# any load-command rewrite and for freshly copied binaries to load SDK dylibs)
if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "Re-signing macOS binaries..."
    find /usr/local/bin -type f -perm +111 2>/dev/null | while read -r drv; do
        case "$(basename "$drv")" in
            indi_*|asi_*|qhy_*|sim_*|test_*|*_test)
                codesign --force --sign - "$drv" 2>/dev/null || true
                ;;
        esac
    done
    echo "Re-signing done."
fi
