#!/bin/bash
set -e

# Setup directories
SCRIPT_DIR=$(dirname $(readlink -f "$0"))
BASE_DIR=$(pwd)
# If running from inside scripts/, adjust BASE_DIR
if [[ "$BASE_DIR" == */scripts ]]; then
    BASE_DIR=$(dirname "$BASE_DIR")
fi

WORKTREE_ROOT="$BASE_DIR/worktree"

function usage() {
    echo "Usage: $0 <driver_name> [target_directory]"
    echo ""
    echo "Prepares a slim, Git-native worktree for developing a single INDI driver"
    echo "using Git Sparse Checkout (industry best practice for monorepos)."
    echo ""
    echo "Example: $0 playerone ./dev-playerone"
    exit 1
}

if [ -z "$1" ]; then
    usage
fi

DRIVER_NAME="$1"
# Ensure the name is clean (no indi- prefix for internal logic, we'll check both)
RAW_NAME="${DRIVER_NAME#indi-}"

TARGET_DIR="${2:-$WORKTREE_ROOT/indi-$RAW_NAME}"
ABS_TARGET_DIR=$(readlink -f "$TARGET_DIR")

echo ">>> Creating slim Git worktree for $DRIVER_NAME in $ABS_TARGET_DIR..."

# 1. Clone the repo with blob filtering (blobless clone)
# This downloads all commit history but NO file contents.
# File contents are downloaded only when checked out.
if [ ! -d "$ABS_TARGET_DIR" ]; then
    echo "Cloning indi-3rdparty (blobless)..."
    git clone --filter=blob:none --no-checkout https://github.com/indilib/indi-3rdparty.git "$ABS_TARGET_DIR"
fi

pushd "$ABS_TARGET_DIR" > /dev/null

# 2. Initialize sparse-checkout
echo "Initializing sparse-checkout..."
git sparse-checkout init --cone

# 3. Define directories to fetch
# We need:
# - The driver source (indi-driver or lib-driver)
# - The debian metadata for that driver
# - Essential common build files
echo "Configuring sparse-checkout paths..."

# Determine actual folder names in the repo
# We'll fetch the directory listing from origin to be sure
DRIVER_PATH="indi-$RAW_NAME"
# Some might be lib- something, but most are indi-
# In sparse-checkout, we can just set the patterns we expect.

git sparse-checkout set \
    "cmake_modules" \
    "debian/indi-$RAW_NAME" \
    "debian/lib$RAW_NAME" \
    "indi-$RAW_NAME" \
    "lib$RAW_NAME" \
    "CMakeLists.txt" \
    "README.md" \
    "LICENSE" \
    "tools"

# 4. Checkout the files
echo "Checking out files..."
git checkout master

# 5. Final check
if [ ! -d "indi-$RAW_NAME" ] && [ ! -d "lib$RAW_NAME" ]; then
    echo "Warning: Could not find source for $DRIVER_NAME in the repository."
    echo "Currently checked out patterns:"
    git sparse-checkout list
fi

popd > /dev/null

echo "================================================================================"
echo " GIT SPARSE WORKTREE READY"
echo "================================================================================"
echo "Location: $ABS_TARGET_DIR"
echo ""
echo "This is a FULL Git repository, but only files for $DRIVER_NAME are on disk."
echo "You can use git commit/push/pull normally."
echo ""
echo "To build this driver using the deb-builder, add it to LOCAL_DEVELOPMENT_PACKAGES"
echo "in packages.conf as:"
echo "  \"$DRIVER_NAME:$ABS_TARGET_DIR/indi-$RAW_NAME\""
echo "================================================================================"
