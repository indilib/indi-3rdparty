#!/bin/bash

# The Atik SDK uncompresses as a list of files prefixed with Windows backslashes

if [ -z "${1:-}" -o ! -d "${1:-}" -o ! -f "${1:-}/include/AtikCameras.h" ]
then
  echo "Picks Atik libraries and headers from an extracted SDK and moves them here in ./libatik" >&2
  echo "Usage: $0 <sdk folder>" >&2
  exit 2
fi

archive="$1"

set -eux -o pipefail

copy_lib() {
  find $archive -wholename "$1" | \
    while read -r f
    do
      cp "$f" "./$2/$(basename "$f" | sed -e 's/.*\\//' -e 's/so$/bin/')"
    done
}

copy_lib '*ARM/64/With*.so' 'arm64'
copy_lib '*ARM/32/With*.so' 'armhf'
copy_lib '*Linux/64/With*.so' 'x64'
copy_lib '*Linux/32/With*.so' 'x86'

cp "$1/lib/macos/libatikcameras.dylib" "./mac/libatikcameras.bin"

cp "$1/include/AtikCameras.h" .
cp "$1/include/AtikDefs.h" .
