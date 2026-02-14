#!/bin/bash
set -e

# Load configuration
SCRIPT_DIR=$(dirname $(readlink -f "$0"))
CONFIG_FILE="$SCRIPT_DIR/packages.conf"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: $CONFIG_FILE not found!"
    exit 1
fi
source "$CONFIG_FILE"

# Parse CLI arguments (overrides config file)
while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--force-core)
            FORCE_CORE_REBUILD=true
            shift
            ;;
        -p|--packages)
            IFS=',' read -r -a OFFICIAL_3RDPARTY_DRIVERS <<< "$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "  -f, --force-core    Force rebuild of INDI Core even if already installed"
            echo "  -p, --packages PKG  Comma-separated list of 3rd-party drivers to build"
            echo "  -h, --help          Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

# Setup directories
BASE_DIR=$(pwd)
SCRIPT_DIR=$(dirname $(readlink -f "$0"))
# Detect if we are running inside the indi-3rdparty repository
INSIDE_REPO=false
if [ -d "$SCRIPT_DIR/../../.git" ] && [ -f "$SCRIPT_DIR/../../CMakeLists.txt" ]; then
    INSIDE_REPO=true
    REPO_ROOT=$(readlink -f "$SCRIPT_DIR/../../")
    echo ">>> Detected execution inside indi-3rdparty repository."
fi

mkdir -p "$OUTPUT_DIR"
mkdir -p "$WORKING_DIR"
ABS_OUTPUT_DIR=$(readlink -f "$OUTPUT_DIR")
ABS_WORKING_DIR=$(readlink -f "$WORKING_DIR")



function get_latest_tag() {
    local repo_url=$1
    local tag=$(git ls-remote --tags --refs --sort='v:refname' "$repo_url" | tail -n1 | awk -F/ '{print $3}')
    if [ -z "$tag" ]; then
        tag="master"
    fi
    echo "$tag"
}

function prepare_source() {
    local repo_url=$1
    local target_dir=$2
    local version=$3

    if [ "$version" == "latest" ]; then
        echo "Auto-detecting latest version for $repo_url..." >&2
        version=$(get_latest_tag "$repo_url")
        echo "Detected: $version" >&2
    fi

    if [ ! -d "$target_dir" ]; then
        echo "Cloning $repo_url (version $version)..." >&2
        git clone --depth 1 --branch "$version" "$repo_url" "$target_dir" >&2
    else
        pushd "$target_dir" > /dev/null
        local current_version=$(git describe --tags 2>/dev/null || git rev-parse HEAD)
        if [ "$current_version" == "$version" ]; then
            echo "Already at version $version in $target_dir" >&2
        else
            echo "Updating existing clone in $target_dir..." >&2
            git fetch --tags --depth 1 origin "$version" >&2
            git checkout -f "$version" >&2
            git clean -df >&2
        fi
        popd > /dev/null
    fi
    echo "$version"
}

function progress_bar() {
    local name=$1
    local percent=$2
    local elapsed=$3
    local width=30
    local filled=$(( percent * width / 100 ))
    local empty=$(( width - filled ))
    
    local bar=""
    for ((i=0; i<filled; i++)); do bar="${bar}#"; done
    for ((i=0; i<empty; i++)); do bar="${bar}-"; done
    
    local eta="??:??"
    if (( percent > 0 )); then
        local total_est=$(( elapsed * 100 / percent ))
        local remaining=$(( total_est - elapsed ))
        if (( remaining < 0 )); then remaining=0; fi
        eta=$(printf "%02d:%02d" $((remaining/60)) $((remaining%60)))
    fi
    
    local min=$((elapsed/60))
    local sec=$((elapsed%60))
    
    # Format: Building [pkg] [#####-----]  50% (00:02<00:02)
    printf "\rBuilding %-15s [%s] %3d%% (%02d:%02d<%s)\e[K" "$name" "$bar" "$percent" "$min" "$sec" "$eta"
}

function show_progress() {
    local pid=$1
    local name=$2
    local log_file=$3
    local start_time=$(date +%s)
    local last_percent=0
    
    while kill -0 "$pid" 2>/dev/null; do
        local current_time=$(date +%s)
        local elapsed=$((current_time - start_time))
        
        # Try to find the last percentage in the log file
        local percent=$(tail -n 100 "$log_file" 2>/dev/null | grep -oa "\[ *[0-9]\+%\]" | tail -n 1 | grep -oa "[0-9]\+")
        
        if [[ -n "$percent" ]]; then
            last_percent=$percent
        fi
        
        progress_bar "$name" "$last_percent" "$elapsed"
        sleep 1
    done
    # Clear the progress bar line
    printf "\r\e[K"
}

function build_package() {
    local name=$1
    local src_dir=$2
    local log_file="$ABS_WORKING_DIR/${name}_build.log"
    
    echo "================================================================================"
    echo " BUILDING: $name"
    echo " SOURCE:   $src_dir"
    echo " LOG:      $log_file"
    echo "================================================================================"

    pushd "$src_dir" > /dev/null
    
    # Check if debian directory exists
    if [ ! -d "debian" ]; then
        echo "Error: No debian directory found in $name."
        popd > /dev/null
        return 1
    fi

    # Check for build dependencies
    if ! dpkg-checkbuilddeps >/dev/null 2>&1; then
        echo ">>> Error: Missing build dependencies for $name:"
        dpkg-checkbuilddeps 2>&1 | sed 's/dpkg-checkbuilddeps: błąd: unmet build dependencies: //' | sed 's/dpkg-checkbuilddeps: error: unmet build dependencies: //'
        echo ""
        echo "Please install them using:"
        echo "  sudo apt-get install $(dpkg-checkbuilddeps 2>&1 | sed 's/.* dependencies: //' | sed 's/([^)]*)//g')"
        echo ""
        popd > /dev/null
        return 1
    fi

    # Build the package
    # We use stdbuf to ensure output is not buffered
    stdbuf -oL -eL debuild -us -uc -b -j"$BUILD_THREADS" > "$log_file" 2>&1 &
    local build_pid=$!
    
    show_progress "$build_pid" "$name" "$log_file"
    
    wait "$build_pid"
    local status=$?
    
    popd > /dev/null
    
    if [ $status -ne 0 ]; then
        echo ">>> Build FAILED for $name. See log below:"
        echo "--------------------------------------------------------------------------------"
        cat "$log_file"
        echo "--------------------------------------------------------------------------------"
        return $status
    fi

    # Move results to output directory
    # We look for .deb files in the parent of the src_dir (where debuild puts them)
    local parent_dir=$(dirname "$src_dir")
    mv "$parent_dir"/*.deb "$ABS_OUTPUT_DIR/" 2>/dev/null || true
    mv "$parent_dir"/*.changes "$ABS_OUTPUT_DIR/" 2>/dev/null || true
    mv "$parent_dir"/*.buildinfo "$ABS_OUTPUT_DIR/" 2>/dev/null || true
    
    echo "Done building $name."
}

function get_local_deps() {
    local pkg=$1
    local tp_dir=$2
    local control_file="$tp_dir/debian/$pkg/control"
    local deps=()
    
    if [ -f "$control_file" ]; then
        # Extract Build-Depends and check if any match a directory in debian/
        local build_deps=$(sed -n '/^Build-Depends:/,/^[A-Z]/p' "$control_file" | sed 's/^Build-Depends://' | sed '$d' | tr ',' '\n' | sed 's/(.*)//' | awk '{$1=$1;print}')
        for dep in $build_deps; do
            if [ -d "$tp_dir/debian/$dep" ] && [ "$dep" != "$pkg" ]; then
                deps+=("$dep")
            fi
        done
    fi
    echo "${deps[@]}"
}

function check_installed() {
    dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "ok installed"
}

function get_installed_version() {
    # Extract version, remove epoch if present (e.g. 1:2.0.0 -> 2.0.0)
    dpkg-query -W -f='${Version}' "$1" 2>/dev/null | sed 's/^[0-9]://' | cut -d- -f1
}

function get_debian_version() {
    local pkg=$1
    local tp_dir=$2
    local changelog="$tp_dir/debian/$pkg/changelog"
    if [ -f "$changelog" ]; then
        head -n 1 "$changelog" | grep -o "(.*)" | tr -d "()" | cut -d- -f1
    else
        echo "0.0.0"
    fi
}

function needs_update() {
    local pkg=$1
    local upstream_ver=$2
    local tp_dir=$3  # Optional: for 3rd party drivers
    
    if ! check_installed "$pkg"; then
        return 0 # Not installed, needs build
    fi
    
    local installed_ver=$(get_installed_version "$pkg")
    local target_ver=""

    if [ -n "$tp_dir" ]; then
        # For 3rd party, the target version is defined in its own debian/changelog
        target_ver=$(get_debian_version "$pkg" "$tp_dir")
        echo ">>> Checking $pkg: installed=$installed_ver, debian_source=$target_ver" >&2
    else
        # For core, we use the git tag
        target_ver="${upstream_ver#v}"
        echo ">>> Checking $pkg: installed=$installed_ver, upstream_tag=$target_ver" >&2
    fi
    
    # If installed version is less than target version, return 0 (needs update)
    if dpkg --compare-versions "$installed_ver" lt "$target_ver"; then
        echo ">>> Update available for $pkg: $installed_ver -> $target_ver" >&2
        return 0
    fi
    
    return 1 # Already up to date
}

function build_package_tp() {
    local name=$1
    local tp_dir=$2
    local build_tmp="$ABS_WORKING_DIR/tp-build-$name"

    echo "Preparing temporary build directory for $name..."
    rm -rf "$build_tmp"
    mkdir -p "$build_tmp"
    
    # Symlink everything from tp_dir except debian and build_area (if any)
    for item in "$tp_dir"/*; do
        local basename=$(basename "$item")
        if [ "$basename" != "debian" ] && [ "$basename" != "build_area" ]; then
            ln -s "$item" "$build_tmp/$basename"
        fi
    done
    
    # Symlink the specific debian folder
    if [ -d "$tp_dir/debian/$name" ]; then
        ln -s "$tp_dir/debian/$name" "$build_tmp/debian"
    else
        echo "Error: Debian directory $tp_dir/debian/$name not found."
        rm -rf "$build_tmp"
        return 1
    fi
    
    local status=0
    build_package "$name" "$build_tmp" || status=$?
    
    # Clean up
    rm -rf "$build_tmp"
    return $status
}

# --- Stage 1: INDI Core ---
if [ "$BUILD_INDI_CORE" = true ]; then
    CORE_DIR="$ABS_WORKING_DIR/indi-core"
    CORE_VERSION=$(prepare_source "https://github.com/indilib/indi.git" "$CORE_DIR" "$INDI_CORE_VERSION")

    if needs_update "libindi-dev" "$CORE_VERSION" || [ "$FORCE_CORE_REBUILD" = true ]; then
        if [ "$FORCE_CORE_REBUILD" = true ]; then
            echo ">>> Stage 1: Forced rebuild of INDI Core enabled."
        else
            echo ">>> Stage 1: Building INDI Core ($CORE_VERSION)..."
        fi
        
        if ! build_package "indi-core" "$CORE_DIR"; then
            echo ">>> Stage 1 failed. Please address the missing dependencies above and try again."
            exit 1
        fi
        
        echo ""
        echo "================================================================================"
        echo " STAGE 1 COMPLETE: INDI Core built"
        echo "================================================================================"
        echo "Please install the core packages before proceeding to Stage 2:"
        echo "  sudo dpkg -i dist/libindi*.deb dist/indi-bin*.deb"
        echo ""
        echo "After installation, run this script again."
        echo "================================================================================"
        exit 0
    else
        echo ">>> Stage 1: libindi-dev is already installed and up to date."
    fi
else
    echo ">>> Stage 1: INDI Core build disabled in configuration. Skipping."
    # We still need to check if it's installed because drivers depend on it
    if ! check_installed "libindi-dev"; then
        echo "Error: libindi-dev is not installed and core build is disabled."
        echo "Please install INDI core packages or enable BUILD_INDI_CORE in packages.conf."
        exit 1
    fi
fi

# --- Stage 2: 3rd Party Libraries ---
if [ "$INSIDE_REPO" = true ]; then
    TP_REPO_DIR="$REPO_ROOT"
    TP_VERSION="local-repo"
    echo ">>> Stage 2: Using local repository as source."
else
    TP_REPO_DIR="$ABS_WORKING_DIR/indi-3rdparty"
    TP_VERSION=$(prepare_source "https://github.com/indilib/indi-3rdparty.git" "$TP_REPO_DIR" "$INDI_3RDPARTY_VERSION")
fi
echo ">>> Processing Official 3rd Party Drivers ($TP_VERSION)..."

# Detect required libraries for selected drivers
libs_to_build=()
drivers_to_process=()

for pkg_name in "${OFFICIAL_3RDPARTY_DRIVERS[@]}"; do
    pkg="$pkg_name"
    # Try with indi- prefix if not found
    if [ ! -d "$TP_REPO_DIR/debian/$pkg" ]; then
        if [ -d "$TP_REPO_DIR/debian/indi-$pkg" ]; then
            pkg="indi-$pkg"
        fi
    fi

    if [ -d "$TP_REPO_DIR/debian/$pkg" ]; then
        driver_needs_update=0
        if needs_update "$pkg" "$TP_VERSION" "$TP_REPO_DIR"; then
            driver_needs_update=1
        fi

        dep_needs_update=0
        deps=$(get_local_deps "$pkg" "$TP_REPO_DIR")
        for dep in $deps; do
            if needs_update "$dep" "$TP_VERSION" "$TP_REPO_DIR"; then
                dep_needs_update=1
                libs_to_build+=("$dep")
            fi
        done

        if [ "$driver_needs_update" -eq 1 ] || [ "$dep_needs_update" -eq 1 ]; then
            drivers_to_process+=("$pkg")
            # If any dependency is being rebuilt, we should probably ensure all local deps 
            # are considered for the build list if they are not installed or are older
            for dep in $deps; do
                if needs_update "$dep" "$TP_VERSION" "$TP_REPO_DIR"; then
                    libs_to_build+=("$dep")
                fi
            done
        else
            echo ">>> Driver $pkg and its dependencies are already up to date."
        fi
    else
        echo "Warning: Driver $pkg_name (or indi-$pkg_name) not found in 3rdparty repository."
    fi
done

# Deduplicate libraries
libs_to_build=($(echo "${libs_to_build[@]}" | tr ' ' '\n' | sort -u))

if [ ${#libs_to_build[@]} -gt 0 ]; then
    echo ">>> Stage 2: Building required libraries: ${libs_to_build[*]}"
    built_libs=()
    for lib in "${libs_to_build[@]}"; do
        if build_package_tp "$lib" "$TP_REPO_DIR"; then
            built_libs+=("$lib")
        else
            echo ">>> Stage 2: Failed to build library $lib. Please address the missing dependencies above."
            exit 1
        fi
    done
    
    echo ""
    echo "================================================================================"
    echo " STAGE 2 COMPLETE: Libraries built"
    echo "================================================================================"
    echo "Please install the following libraries before proceeding to Stage 3:"
    for lib in "${built_libs[@]}"; do
        echo "  - $lib"
    done
    echo ""
    echo "Please install them using:"
    echo "  sudo dpkg -i $(printf 'dist/%s*.deb ' "${built_libs[@]}")"
    echo ""
    echo "After installation, run this script again."
    echo "================================================================================"
    exit 0
else
    echo ">>> Stage 2: All required libraries are installed. Proceeding to Stage 3."
fi

# --- Stage 3: 3rd Party Drivers & Local Packages ---
echo ">>> Stage 3: Building Official Drivers..."
built_drivers=()
for driver in "${drivers_to_process[@]}"; do
    if ! build_package_tp "$driver" "$TP_REPO_DIR"; then
        echo ">>> Stage 3: Failed to build driver $driver. Please address the missing dependencies above."
        exit 1
    fi
    built_drivers+=("$driver")
done

echo ">>> Stage 3: Processing Local Development Packages..."
for entry in "${LOCAL_DEVELOPMENT_PACKAGES[@]}"; do
    name="${entry%%:*}"
    path="${entry##*:}"
    
    # Resolve relative path if necessary
    if [[ "$path" != /* ]]; then
        path="$BASE_DIR/$path"
    fi

    if [ -d "$path" ]; then
        # Copy to working dir to avoid polluting local source with build artifacts
        LOCAL_BUILD_DIR="$ABS_WORKING_DIR/local-$name"
        rm -rf "$LOCAL_BUILD_DIR"
        mkdir -p "$LOCAL_BUILD_DIR"
        cp -r "$path/." "$LOCAL_BUILD_DIR/"
        
        if ! build_package "local-$name" "$LOCAL_BUILD_DIR"; then
            echo ">>> Stage 3: Failed to build local package $name."
            exit 1
        fi
        built_drivers+=("$name")
    else
        echo "Error: Local path $path for $name does not exist!"
    fi
done

echo "================================================================================"
echo " BUILD COMPLETE"
echo " Packages are located in: $OUTPUT_DIR"
echo "================================================================================"
if [ ${#built_drivers[@]} -gt 0 ]; then
    echo "To install the built drivers, run:"
    echo "  sudo dpkg -i $(printf 'dist/%s*.deb ' "${built_drivers[@]}")"
    echo "================================================================================"
fi
