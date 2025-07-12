#!/bin/bash

# The Atik SDK uncompresses as a list of files prefixed with Windows backslashes

if [ -z "${1:-}" -o ! -d "${1:-}" -o ! -f "${1:-}/include/AtikCameras.h" ]
then
  echo "Picks Atik libraries and headers from an extracted SDK and moves them here in ./libatik" >&2
  echo "Usage: $0 <sdk folder>" >&2
  exit 2
fi

archive="$1"

set -eu -o pipefail

if [ -n "${SDK2LIBATIK_DEBUG:+n}" ] ; then set -x ; fi

copy_lib() {
  find $archive -wholename "$1" | \
    while read -r f
    do
      local dest="./$2/$(basename "$f" | sed -e 's/.*\\//' -e 's/so$/bin/')"
      echo "Copying '$f' to '$dest'..."
      cp "$f" "$dest"
    done
}

if [ -z $(find $archive -name 'With*') ] ; then
  copy_lib '*ARM/64/*.so' 'arm64'
  copy_lib '*ARM/32/*.so' 'armhf'
  copy_lib '*Linux/64/*.so' 'x64'
  copy_lib '*Linux/32/*.so' 'x86'
else
  copy_lib '*ARM/64/With*.so' 'arm64'
  copy_lib '*ARM/32/With*.so' 'armhf'
  copy_lib '*Linux/64/With*.so' 'x64'
  copy_lib '*Linux/32/With*.so' 'x86'
fi

if [ -z $(find $archive -name '*dylib') ] ; then
  echo "WARNING: archive '$archive' does not seem to contain any MacOS library!" >&2
else
  cp "$1/lib/macos/libatikcameras.dylib" "./mac/libatikcameras.bin"
fi

cp "$1/include/AtikCameras.h" .
cp "$1/include/AtikDefs.h" .
