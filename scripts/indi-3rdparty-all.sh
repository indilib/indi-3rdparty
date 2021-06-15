#!/bin/bash

set -e

command -v realpath >/dev/null 2>&1 || realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

SRCS=$(dirname $(realpath $0))/..

${SRCS}/scripts/googletest-build.sh
${SRCS}/scripts/googletest-install.sh

${SRCS}/scripts/indi-3rdparty-libs-build.sh
${SRCS}/scripts/indi-3rdparty-libs-install.sh

${SRCS}/scripts/indi-3rdparty-build.sh
${SRCS}/scripts/indi-3rdparty-install.sh
