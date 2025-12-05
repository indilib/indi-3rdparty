#!/usr/bin/env python3
"""
Synchronize ZWO ASI SDK files to libasi

This script automatically detects the SDK type from the ZIP filename and
synchronizes the appropriate libraries and headers to the libasi directory.

Usage:
    python3 sync_asi_sdk.py /path/to/SDK.zip
    python3 sync_asi_sdk.py /path/to/ASI_Camera_SDK.zip
    python3 sync_asi_sdk.py /path/to/EFW_SDK.zip
    python3 sync_asi_sdk.py /path/to/CAA_SDK.zip
    python3 sync_asi_sdk.py /path/to/EAF_SDK.zip

You can also specify the SDK type explicitly:
    python3 sync_asi_sdk.py --type camera /path/to/SDK.zip
    python3 sync_asi_sdk.py --type efw /path/to/SDK.zip
    python3 sync_asi_sdk.py --type caa /path/to/SDK.zip
    python3 sync_asi_sdk.py --type eaf /path/to/SDK.zip
"""

import sys
import os
import re
import shutil
import zipfile
import tarfile
import tempfile
import subprocess
import argparse
from pathlib import Path

# SDK Configuration
SDK_CONFIGS = {
    'camera': {
        'lib_name': 'libASICamera2',
        'sdk_lib_name': 'libASICamera2',
        'cmake_var': 'ASICAM',
        'header_files': ['ASICamera2.h'],
        'tar_pattern': r'ASI_.*_SDK_V.*\.tar\.bz2',
        'zip_patterns': ['ASI_Camera', 'ASI_SDK', 'ASI_linux', 'ASI_mac'],
        'description': 'ASI Camera'
    },
    'efw': {
        'lib_name': 'libEFWFilter',
        'sdk_lib_name': 'libEFWFilter',
        'cmake_var': 'ASIEFW',
        'header_files': ['EFW_filter.h'],
        'tar_pattern': r'EFW_.*_SDK_V.*\.tar\.bz2',
        'zip_patterns': ['EFW_SDK', 'EFW_Linux', 'EFW_macOS'],
        'description': 'EFW Filter'
    },
    'caa': {
        'lib_name': 'libCAARotator',
        'sdk_lib_name': 'libCAA',
        'cmake_var': 'ASICAA',
        'header_files': ['CAA_API.h'],
        'tar_pattern': r'CAA_.*_SDK_V.*\.tar\.bz2',
        'zip_patterns': ['CAA_SDK', 'CAA_Linux', 'CAA_macOS'],
        'description': 'CAA Rotator'
    },
    'eaf': {
        'lib_name': 'libEAFFocuser',
        'sdk_lib_name': 'libEAFFocuser',
        'cmake_var': 'ASIEAF',
        'header_files': ['EAF_focuser.h'],
        'tar_pattern': r'EAF_.*_SDK_V.*\.tar\.bz2',
        'zip_patterns': ['EAF_SDK', 'EAF_Linux', 'EAF_macOS'],
        'description': 'EAF Focuser'
    }
}

# ANSI Colors
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    MAGENTA = '\033[0;35m'
    NC = '\033[0m'  # No Color


def print_info(msg):
    print(f"{Colors.BLUE}{msg}{Colors.NC}")


def print_success(msg):
    print(f"{Colors.GREEN}{msg}{Colors.NC}")


def print_warning(msg):
    print(f"{Colors.YELLOW}{msg}{Colors.NC}")


def print_error(msg):
    print(f"{Colors.RED}{msg}{Colors.NC}")


def print_header(msg):
    print(f"{Colors.MAGENTA}{'=' * 60}{Colors.NC}")
    print(f"{Colors.MAGENTA}{msg}{Colors.NC}")
    print(f"{Colors.MAGENTA}{'=' * 60}{Colors.NC}")


def detect_sdk_type(zip_filename):
    """Detect SDK type from ZIP filename"""
    basename = os.path.basename(zip_filename).upper()
    
    for sdk_type, config in SDK_CONFIGS.items():
        for pattern in config['zip_patterns']:
            if pattern.upper() in basename:
                return sdk_type
    
    return None


def extract_version_from_filename(filename):
    """Extract version from filename like ASI_linux_mac_SDK_V1.40.tar.bz2"""
    match = re.search(r'V(\d+\.\d+(?:\.\d+)?)', filename)
    if match:
        return match.group(1)
    return None


def find_file_by_pattern(directory, pattern):
    """Find first file matching pattern in directory"""
    for root, dirs, files in os.walk(directory):
        for file in files:
            if re.match(pattern, file):
                return os.path.join(root, file)
    return None


def process_libraries(sdk_dir, libasi_dir, sdk_lib_name, dest_lib_name, version):
    """Process library files for all architectures
    
    Args:
        sdk_dir: SDK directory path
        libasi_dir: libasi directory path
        sdk_lib_name: Library name in SDK (e.g., libCAA)
        dest_lib_name: Final library name for libasi (e.g., libCAARotator)
        version: Version string
    """
    print_warning("Processing library files...")
    
    architectures = ["x64", "x86", "armv6", "armv7", "armv8", "mac", "mac_x64", "mac_arm64"]
    processed_count = 0
    
    for arch in architectures:
        src_dir = os.path.join(sdk_dir, "lib", arch)
        dest_dir = os.path.join(libasi_dir, arch)
        
        # Skip if source doesn't exist
        if not os.path.isdir(src_dir):
            continue
        
        # Handle fallback: if SDK has "mac" but libasi expects "mac_x64"
        # (older SDKs use "mac" naming, newer structure uses "mac_x64")
        if not os.path.isdir(dest_dir):
            if arch == "mac" and os.path.isdir(os.path.join(libasi_dir, "mac_x64")):
                dest_dir = os.path.join(libasi_dir, "mac_x64")
                print_info(f"  Note: Copying 'mac' to 'mac_x64' (SDK uses older naming)")
            else:
                print_warning(f"  Skipping {arch} (destination directory not found)")
                continue
        
        # Find the versioned library file - try multiple patterns
        # Linux uses .so, macOS uses .dylib
        # Search using SDK library name
        so_file = None
        for file in os.listdir(src_dir):
            # Try exact match with version, or just extension (both .so and .dylib)
            if (file.startswith(sdk_lib_name) and 
                ((file == f"{sdk_lib_name}.so.{version}" or 
                  file == f"{sdk_lib_name}.dylib.{version}" or
                  file == f"{sdk_lib_name}.so" or
                  file == f"{sdk_lib_name}.dylib" or
                  re.match(rf"{re.escape(sdk_lib_name)}\.so\.\d+(\.\d+)*", file) or
                  re.match(rf"{re.escape(sdk_lib_name)}\.dylib\.\d+(\.\d+)*", file)))):
                so_file = os.path.join(src_dir, file)
                break
        
        if not so_file:
            print_warning(f"  Skipping {arch} (library file not found)")
            continue
        
        # Copy and rename to .bin using destination library name
        dest_file = os.path.join(dest_dir, f"{dest_lib_name}.bin")
        shutil.copy2(so_file, dest_file)
        # Set permissions to 664 (rw-rw-r--)
        os.chmod(dest_file, 0o664)
        print_success(f"  ✓ {arch}: Copied {os.path.basename(so_file)} -> {dest_lib_name}.bin")
        processed_count += 1
    
    if processed_count == 0:
        print_error("  Error: No library files were processed!")
        return False
    
    return True


def process_headers(sdk_dir, libasi_dir, header_files):
    """Process header files"""
    print_warning("Processing header files...")
    
    include_dir = os.path.join(sdk_dir, "include")
    
    if not os.path.isdir(include_dir):
        print_warning("  Warning: include directory not found")
        return True
    
    processed_count = 0
    for file in os.listdir(include_dir):
        # Skip dot files
        if file.startswith('.'):
            continue
        
        src_file = os.path.join(include_dir, file)
        
        # Skip directories
        if not os.path.isfile(src_file):
            continue
        
        dest_file = os.path.join(libasi_dir, file)
        
        # Copy header file
        shutil.copy2(src_file, dest_file)
        # Set permissions to 664 (rw-rw-r--)
        os.chmod(dest_file, 0o664)
        print_success(f"  ✓ Copied {file}")
        processed_count += 1
        
        # Run dos2unix if available
        if shutil.which('dos2unix'):
            try:
                subprocess.run(['dos2unix', '-q', dest_file], 
                             stderr=subprocess.DEVNULL, check=False)
            except:
                pass
    
    if processed_count == 0:
        print_warning("  Warning: No header files were copied")
    
    return True


def update_cmake(libasi_dir, cmake_var, version):
    """Update CMakeLists.txt with new version"""
    print_warning("Updating CMakeLists.txt...")
    
    cmake_file = os.path.join(libasi_dir, "CMakeLists.txt")
    
    if not os.path.isfile(cmake_file):
        print_error("  Error: CMakeLists.txt not found")
        return False
    
    # Read the file
    with open(cmake_file, 'r') as f:
        content = f.read()
    
    # Extract SOVERSION (major version)
    soversion = version.split('.')[0]
    
    # Update VERSION
    content = re.sub(
        rf'set \({cmake_var}_VERSION "[0-9.]+"\)',
        f'set ({cmake_var}_VERSION "{version}")',
        content
    )
    
    # Update SOVERSION
    content = re.sub(
        rf'set \({cmake_var}_SOVERSION "[0-9]+"\)',
        f'set ({cmake_var}_SOVERSION "{soversion}")',
        content
    )
    
    # Write back
    with open(cmake_file, 'w') as f:
        f.write(content)
    
    print_success(f"  ✓ Updated {cmake_var}_VERSION to {version}")
    print_success(f"  ✓ Updated {cmake_var}_SOVERSION to {soversion}")
    
    return True


def fix_macos_libs(script_dir, libasi_dir):
    """Run fix_macos_libs.py script"""
    print_warning("Fixing macOS libraries...")
    
    fix_script = os.path.join(script_dir, "fix_macos_libs.py")
    
    if not os.path.isfile(fix_script):
        print_warning("  Warning: fix_macos_libs.py not found")
        return
    
    try:
        subprocess.run([sys.executable, fix_script, libasi_dir, '--fix'], 
                      check=True)
    except subprocess.CalledProcessError as e:
        print_warning(f"  Warning: fix_macos_libs.py returned error code {e.returncode}")


def sync_sdk(zip_file, sdk_type):
    """Synchronize SDK files to libasi"""
    
    config = SDK_CONFIGS[sdk_type]
    
    print_header(f"ASI {config['description']} SDK Synchronization")
    
    # Validate ZIP file exists
    if not os.path.isfile(zip_file):
        print_error(f"Error: ZIP file not found: {zip_file}")
        return False
    
    # Get paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    libasi_dir = os.path.abspath(os.path.join(script_dir, "..", "libasi"))
    
    # Validate libasi directory exists
    if not os.path.isdir(libasi_dir):
        print_error(f"Error: libasi directory not found: {libasi_dir}")
        return False
    
    print_info(f"SDK Type: {config['description']}")
    print_info(f"ZIP file: {zip_file}")
    print_info(f"Target directory: {libasi_dir}")
    print_info(f"Library: {config['lib_name']}")
    
    # Create temporary directory
    with tempfile.TemporaryDirectory() as temp_dir:
        # Extract ZIP file
        print_warning("Extracting ZIP file...")
        try:
            with zipfile.ZipFile(zip_file, 'r') as zip_ref:
                zip_ref.extractall(temp_dir)
        except Exception as e:
            print_error(f"Error: Failed to extract ZIP file: {e}")
            return False
        
        # Find the tar.bz2 file
        tar_file = find_file_by_pattern(temp_dir, config['tar_pattern'])
        
        if not tar_file:
            print_error(f"Error: Could not find {config['description']} SDK tar.bz2 file in ZIP")
            print_info(f"Looking for pattern: {config['tar_pattern']}")
            return False
        
        print_warning(f"Found: {os.path.basename(tar_file)}")
        
        # Extract version from filename
        version = extract_version_from_filename(os.path.basename(tar_file))
        
        if not version:
            print_error("Error: Could not extract version from filename")
            return False
        
        print_success(f"Detected version: {version}")
        
        # Extract tar.bz2 to a new subdirectory to avoid confusion
        print_warning("Extracting tar.bz2...")
        tar_extract_dir = os.path.join(temp_dir, "sdk_extracted")
        os.makedirs(tar_extract_dir, exist_ok=True)
        try:
            with tarfile.open(tar_file, 'r:bz2') as tar_ref:
                tar_ref.extractall(tar_extract_dir)
        except Exception as e:
            print_error(f"Error: Failed to extract tar.bz2 file: {e}")
            return False
        
        # Find the extracted SDK directory (look in the tar extract dir)
        sdk_dir = None
        for item in os.listdir(tar_extract_dir):
            item_path = os.path.join(tar_extract_dir, item)
            # Check if it's a directory
            if os.path.isdir(item_path):
                # Check if it looks like an SDK directory or has lib/include subdirs
                if (os.path.isdir(os.path.join(item_path, "lib")) or 
                    os.path.isdir(os.path.join(item_path, "include"))):
                    sdk_dir = item_path
                    break
        
        # If not found in subdirectory, maybe the tar extracts directly to tar_extract_dir
        if not sdk_dir:
            if (os.path.isdir(os.path.join(tar_extract_dir, "lib")) or 
                os.path.isdir(os.path.join(tar_extract_dir, "include"))):
                sdk_dir = tar_extract_dir
        
        if not sdk_dir:
            print_error("Error: Could not find extracted SDK directory")
            print_info("Available directories in temp:")
            for item in os.listdir(temp_dir):
                item_path = os.path.join(temp_dir, item)
                if os.path.isdir(item_path):
                    print_info(f"  - {item}")
            return False
        
        print_info(f"SDK directory: {sdk_dir}")
        
        # Process library files
        if not process_libraries(sdk_dir, libasi_dir, config['sdk_lib_name'], config['lib_name'], version):
            return False
        
        # Process header files
        if not process_headers(sdk_dir, libasi_dir, config['header_files']):
            return False
        
        # Update CMakeLists.txt
        if not update_cmake(libasi_dir, config['cmake_var'], version):
            return False
        
        # Fix macOS libraries
        fix_macos_libs(script_dir, libasi_dir)
    
    print_header("Synchronization Complete")
    print_success(f"ASI {config['description']} SDK v{version} has been synchronized to libasi")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Synchronize ZWO ASI SDK files to libasi',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Auto-detect SDK type from filename
  python3 sync_asi_sdk.py /path/to/ASI_Camera_SDK.zip
  python3 sync_asi_sdk.py /path/to/EFW_SDK.zip
  
  # Explicitly specify SDK type
  python3 sync_asi_sdk.py --type camera /path/to/SDK.zip
  python3 sync_asi_sdk.py --type efw /path/to/SDK.zip
  python3 sync_asi_sdk.py --type caa /path/to/SDK.zip
  python3 sync_asi_sdk.py --type eaf /path/to/SDK.zip

Supported SDK types:
  camera  - ASI Camera SDK (libASICamera2)
  efw     - EFW Filter SDK (libEFWFilter)
  caa     - CAA Rotator SDK (libCAARotator)
  eaf     - EAF Focuser SDK (libEAFFocuser)
        '''
    )
    
    parser.add_argument('zip_file', help='Path to SDK ZIP file')
    parser.add_argument('--type', '-t', 
                       choices=['camera', 'efw', 'caa', 'eaf'],
                       help='SDK type (auto-detected from filename if not specified)')
    
    args = parser.parse_args()
    
    # Determine SDK type
    sdk_type = args.type
    if not sdk_type:
        sdk_type = detect_sdk_type(args.zip_file)
        if not sdk_type:
            print_error("Error: Could not detect SDK type from filename")
            print_info("Please specify the SDK type with --type option")
            print_info("Supported types: camera, efw, caa, eaf")
            sys.exit(1)
        print_info(f"Auto-detected SDK type: {sdk_type}")
    
    # Synchronize SDK
    success = sync_sdk(args.zip_file, sdk_type)
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
