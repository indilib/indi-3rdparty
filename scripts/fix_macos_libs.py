#!/usr/bin/env python3
"""
Scan and fix macOS dylibs in INDI 3rd party repository

This script automatically finds and fixes macOS dynamic libraries (.bin and .dylib files)
in the indi-3rdparty repository. It corrects install IDs and library dependencies to use
@rpath for better portability.

Requirements:
    pip install macholib

Usage:
    # Scan only (no changes) - check for issues
    python3 fix_macos_libs.py /path/to/indi-3rdparty

    # Scan and automatically fix all issues
    python3 fix_macos_libs.py /path/to/indi-3rdparty --fix

    # Scan specific library directory with verbose output
    python3 fix_macos_libs.py /path/to/indi-3rdparty/libqhy --verbose

    # Show detailed file format information
    python3 fix_macos_libs.py /path/to/indi-3rdparty --diagnose

Common Issues Fixed:
    - Versioned install IDs (e.g., @rpath/lib.20.dylib → @rpath/lib.dylib)
    - Absolute paths in dependencies (e.g., /usr/local/lib/... → @rpath/...)
    - Non-portable loader paths (@loader_path/... → @rpath/...)

Technical Details:
    - Uses macholib's rewriteDataForCommand() for safe Mach-O modifications
    - Handles fat/universal binaries (x86_64 + arm64)
    - Works on both Linux and macOS
"""

import sys
import os
import glob
import struct
import subprocess
from pathlib import Path
from macholib import MachO, mach_o
from macholib.util import is_platform_file

class Colors:
    """ANSI color codes for terminal output"""
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def detect_file_format(filepath):
    """Detect the format of a file"""
    try:
        with open(filepath, 'rb') as f:
            magic = f.read(4)

            if len(magic) < 4:
                return "EMPTY", "File is empty or too small"

            # Check for common compression formats
            if magic[:2] == b'\x1f\x8b':
                return "GZIP", "File is gzip compressed"
            elif magic[:3] == b'\x42\x5a\x68':
                return "BZIP2", "File is bzip2 compressed"
            elif magic[:4] == b'\x50\x4b\x03\x04':
                return "ZIP", "File is zip compressed"
            elif magic[:4] == b'\xfd\x37\x7a\x58':
                return "XZ", "File is xz compressed"

            # Check for Mach-O formats
            # Universal/Fat binary
            if magic[:4] in (b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'):
                return "MACHO_FAT", "Universal (fat) Mach-O binary"
            # 64-bit Mach-O
            elif magic[:4] in (b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe'):
                return "MACHO_64", "64-bit Mach-O binary"
            # 32-bit Mach-O
            elif magic[:4] in (b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe'):
                return "MACHO_32", "32-bit Mach-O binary"
            # Check for all zeros (corrupted or placeholder file)
            elif magic == b'\x00\x00\x00\x00':
                # Read more to confirm
                f.seek(0)
                sample = f.read(1024)
                if all(b == 0 for b in sample):
                    return "EMPTY", "File contains only null bytes (placeholder or corrupted)"
                else:
                    return "UNKNOWN", f"Unknown format (magic: {magic.hex()})"
            else:
                return "UNKNOWN", f"Unknown format (magic: {magic.hex()})"
    except Exception as e:
        return "ERROR", f"Could not read file: {e}"

def get_dylib_id(dylib_path):
    """Get the current install ID of a dylib"""
    try:
        macho = MachO.MachO(dylib_path)
        for header in macho.headers:
            for load_cmd, cmd, data in header.commands:
                if load_cmd.cmd == mach_o.LC_ID_DYLIB:
                    return data.decode('utf-8').rstrip('\x00')
        return None
    except (OSError, ValueError, struct.error) as e:
        # Handle files that macholib can't fully parse (stub libraries)
        # Try reading install ID directly
        install_id = read_install_id_raw(dylib_path)
        if install_id:
            return install_id
        return None
    except Exception as e:
        # Last resort - try raw reading
        install_id = read_install_id_raw(dylib_path)
        if install_id:
            return install_id
        return None

def get_dylib_links(dylib_path):
    """Get all library dependencies"""
    try:
        macho = MachO.MachO(dylib_path)
        links = []
        for header in macho.headers:
            for load_cmd, cmd, data in header.commands:
                if load_cmd.cmd in (mach_o.LC_LOAD_DYLIB, mach_o.LC_LOAD_WEAK_DYLIB):
                    link = data.decode('utf-8').rstrip('\x00')
                    links.append(link)
        return links
    except (OSError, ValueError, struct.error) as e:
        # Handle files that macholib can't fully parse
        return []
    except Exception as e:
        return []

def get_library_name_from_file(filename):
    """
    Extract the library name from filename.
    Examples:
      libqhyccd.bin -> libqhyccd
      libqhyccd.dylib -> libqhyccd
      libASICamera2.bin -> libASICamera2
      libqhyccd.20.bin -> libqhyccd (strip version numbers from .bin files)
      libqhyccd.20.dylib -> libqhyccd.20 (keep version for .dylib files)
    """
    # Remove .bin or .dylib extension
    if filename.endswith('.bin'):
        name = filename.replace('.bin', '')
        # Check if there are version numbers (e.g., libqhyccd.20)
        parts = name.split('.')
        if len(parts) > 1 and any(c.isdigit() for c in parts[-1]):
            # Has version number, remove it
            name = '.'.join(parts[:-1])
    elif filename.endswith('.dylib'):
        # For .dylib files, just remove the extension
        # Keep any version numbers as they might be part of the library name
        name = filename.replace('.dylib', '')
    else:
        name = filename

    return name

def read_install_id_raw(dylib_path):
    """
    Read install ID directly from Mach-O file without full parsing.
    This works even for stub libraries that macholib can't fully parse.
    """
    try:
        with open(dylib_path, 'rb') as f:
            # Read magic number
            magic = struct.unpack('<I', f.read(4))[0]

            # Determine if it's 64-bit or 32-bit, and endianness
            is_64bit = magic in (0xcffaedfe, 0xfeedface)  # 64-bit little/big endian
            is_swap = magic in (0xfefaedfe, 0xcefaedfe)  # Need byte swap

            if magic == 0xcafebabe or magic == 0xbebafeca:
                # Universal/Fat binary - skip to first architecture
                f.seek(4)  # Skip magic
                nfat_arch = struct.unpack('>I', f.read(4))[0]
                # Read first architecture offset
                f.read(4)  # cpu type
                f.read(4)  # cpu subtype
                offset = struct.unpack('>I', f.read(4))[0]
                f.seek(offset)
                magic = struct.unpack('<I', f.read(4))[0]
                is_64bit = magic in (0xcffaedfe, 0xfeedface)

            # Skip to load commands
            if is_64bit:
                f.seek(f.tell() + 24)  # Skip rest of header
            else:
                f.seek(f.tell() + 24)  # Skip rest of header

            # Read number of load commands
            f.seek(4 if is_64bit else 4, 1)  # Skip cputype, cpusubtype, filetype, ncmds
            ncmds_pos = f.tell() - 12
            f.seek(ncmds_pos)
            f.read(12)  # Skip to ncmds
            ncmds = struct.unpack('<I', f.read(4))[0]
            sizeofcmds = struct.unpack('<I', f.read(4))[0]

            # Start of load commands
            if is_64bit:
                lc_start = 32
            else:
                lc_start = 28

            f.seek(lc_start)

            # Search for LC_ID_DYLIB command (0xd)
            for _ in range(ncmds):
                pos = f.tell()
                cmd = struct.unpack('<I', f.read(4))[0]
                cmdsize = struct.unpack('<I', f.read(4))[0]

                if cmd == 0xd:  # LC_ID_DYLIB
                    # Read the string offset (relative to this load command)
                    string_offset = struct.unpack('<I', f.read(4))[0]
                    # Seek to string position
                    f.seek(pos + string_offset)
                    # Read until null terminator
                    install_id = b''
                    while True:
                        byte = f.read(1)
                        if byte == b'\x00' or not byte:
                            break
                        install_id += byte
                    return install_id.decode('utf-8')

                # Move to next load command
                f.seek(pos + cmdsize)

            return None
    except Exception as e:
        return None

def check_dylib(dylib_path, dylib_name):
    """Check if a dylib has issues, return list of problems"""
    problems = []

    # Check file size first - stub libraries are small and should not be modified
    file_size = os.path.getsize(dylib_path)
    if file_size < 50000:  # Less than 50KB - likely a stub
        problems.append(("INFO", f"Stub library ({file_size} bytes) - skipping checks"))
        return problems

    # First check if it's a valid Mach-O file
    file_format, format_desc = detect_file_format(dylib_path)

    if file_format not in ("MACHO_FAT", "MACHO_64", "MACHO_32"):
        if file_format == "EMPTY":
            problems.append(("SKIP", format_desc))
        elif file_format == "GZIP":
            problems.append(("COMPRESSED", "File is gzip compressed - decompress first"))
        elif file_format in ("BZIP2", "XZ", "ZIP"):
            problems.append(("COMPRESSED", f"File is {file_format.lower()} compressed - decompress first"))
        else:
            problems.append(("ERROR", format_desc))
        return problems

    if not is_platform_file(dylib_path):
        problems.append(("ERROR", "Not a valid Mach-O file"))
        return problems

    # Check install ID
    try:
        current_id = get_dylib_id(dylib_path)
        expected_id = f"@rpath/{dylib_name}.dylib"

        if current_id is None:
            problems.append(("WARNING", "Could not read install ID - file may be an object file or have no ID"))
        elif current_id != expected_id:
            problems.append(("ID", f"{current_id} -> {expected_id}"))
    except Exception as e:
        problems.append(("ERROR", f"Error reading install ID: {e}"))
        return problems

    # Check links
    try:
        links = get_dylib_links(dylib_path)
        for link in links:
            # Skip the library itself
            if link.endswith(f'/{dylib_name}.dylib') or link == f'@rpath/{dylib_name}.dylib':
                continue

            # Check for absolute paths
            if link.startswith('/usr/local/') or link.startswith('/opt/'):
                problems.append(("LINK", f"Absolute path: {link}"))
            # Check for @loader_path or @executable_path
            elif link.startswith('@loader_path/') or link.startswith('@executable_path/'):
                problems.append(("LINK", f"Non-portable path: {link}"))
            # Check for versioned libraries  
            elif '@rpath/' in link and '.dylib' in link:
                # List of libraries we know how to fix (matching CMakeLists.txt)
                known_fixable_libs = [
                    'libusb-1.0.0',
                    'libopencv_highgui',
                    'libopencv_videoio', 
                    'libopencv_imgcodecs',
                    'libopencv_imgproc',
                    'libopencv_core'
                ]
                
                # Extract library name from @rpath/libname.dylib
                lib_name = link.split('@rpath/')[-1].replace('.dylib', '')
                
                # If this is a library we know how to fix and it's already in @rpath format, it's OK
                if lib_name in known_fixable_libs:
                    continue
                
                # Otherwise, check if it has version numbers like libqhyccd.20.dylib
                # where the version is added AFTER the base name (e.g., libname.VERSION.dylib)
                parts = lib_name.split('.')
                # Check if the last part before .dylib is purely numeric (likely a version suffix)
                if len(parts) > 1 and parts[-1].isdigit():
                    problems.append(("LINK", f"Versioned library: {link}"))
    except Exception as e:
        problems.append(("ERROR", f"Error reading links: {e}"))

    return problems

def fix_dylib_id(dylib_path, expected_name):
    """Fix the install ID of a dylib to use @rpath using macholib"""
    try:
        expected_id = f"@rpath/{expected_name}.dylib"
        
        macho = MachO.MachO(dylib_path)
        changed = False
        
        for header in macho.headers:
            if header.id_cmd is None:
                continue
            header.rewriteDataForCommand(header.id_cmd, expected_id.encode('utf-8'))
            changed = True
        
        if changed:
            with open(dylib_path, 'r+b') as f:
                macho.write(f)
            return True
        
        return False
    except Exception as e:
        print(f"    {Colors.YELLOW}Warning: Could not fix install ID: {e}{Colors.RESET}")
        return False

def fix_install_id_raw(dylib_path, expected_name):
    """
    DEPRECATED: Fix install ID directly in Mach-O file without full parsing.
    This function is kept for reference but is no longer used.
    Use fix_dylib_id() instead, which calls install_name_tool.
    """
    try:
        expected_id = f"@rpath/{expected_name}.dylib"
        
        with open(dylib_path, 'r+b') as f:
            # Get file size for bounds checking
            f.seek(0, 2)
            file_size = f.tell()
            f.seek(0)
            
            # Read magic number
            magic_bytes = f.read(4)
            if len(magic_bytes) < 4:
                print(f"    {Colors.YELLOW}Warning: File too small to be a Mach-O{Colors.RESET}")
                return False
                
            magic = struct.unpack('<I', magic_bytes)[0]
            
            # Handle universal/fat binaries
            # Note: Fat binary headers are ALWAYS big endian, regardless of magic (0xcafebabe or 0xbebafeca)
            arch_offset = 0
            if magic == 0xcafebabe or magic == 0xbebafeca:
                # Universal/Fat binary
                f.seek(0)
                f.read(4)  # Skip magic
                nfat_arch = struct.unpack('>I', f.read(4))[0]
                
                # Read first architecture offset (always big endian)
                f.read(8)  # Skip cpu type and subtype
                arch_offset = struct.unpack('>I', f.read(4))[0]
                
                if arch_offset >= file_size:
                    print(f"    {Colors.YELLOW}Warning: Invalid offset in fat binary{Colors.RESET}")
                    return False
                    
                f.seek(arch_offset)
                magic_bytes = f.read(4)
                if len(magic_bytes) < 4:
                    return False
                magic = struct.unpack('<I', magic_bytes)[0]

            # Determine if it's 64-bit or 32-bit
            # 0xcffaedfe = MH_MAGIC_64 (Intel 64-bit, little-endian)
            # 0xfeedfacf = MH_MAGIC_64 (ARM64 64-bit, little-endian) - CORRECTED!
            # 0xfeedface = MH_MAGIC_64 (big-endian)
            is_64bit = magic in (0xcffaedfe, 0xfeedfacf, 0xfeedface)
            
            # Calculate header size
            if is_64bit:
                header_size = 32
            else:
                header_size = 28
                
            if arch_offset + header_size > file_size:
                print(f"    {Colors.YELLOW}Warning: File too small for Mach-O header{Colors.RESET}")
                return False

            # Read header to get number of load commands
            f.seek(arch_offset)
            f.read(4)  # Skip magic
            f.read(4)  # Skip cputype
            f.read(4)  # Skip cpusubtype
            f.read(4)  # Skip filetype
            ncmds = struct.unpack('<I', f.read(4))[0]
            sizeofcmds = struct.unpack('<I', f.read(4))[0]
            
            # Validate load commands fit in file
            if arch_offset + header_size + sizeofcmds > file_size:
                print(f"    {Colors.YELLOW}Warning: Load commands extend beyond file size{Colors.RESET}")
                return False

            # Go to start of load commands
            f.seek(arch_offset + header_size)
            
            # Search for LC_ID_DYLIB command
            found_id_cmd = False
            for i in range(ncmds):
                pos = f.tell()
                if pos + 8 > file_size:
                    print(f"    {Colors.YELLOW}Debug: Command {i}: pos + 8 > file_size ({pos + 8} > {file_size}){Colors.RESET}")
                    break
                    
                cmd_bytes = f.read(4)
                cmdsize_bytes = f.read(4)
                
                if len(cmd_bytes) < 4 or len(cmdsize_bytes) < 4:
                    print(f"    {Colors.YELLOW}Debug: Command {i}: Short read on cmd or cmdsize{Colors.RESET}")
                    break
                    
                cmd = struct.unpack('<I', cmd_bytes)[0]
                cmdsize = struct.unpack('<I', cmdsize_bytes)[0]
                
                if cmdsize < 8 or pos + cmdsize > file_size:
                    print(f"    {Colors.YELLOW}Debug: Command {i}: Invalid cmdsize ({cmdsize}) or extends beyond file{Colors.RESET}")
                    break

                if cmd == 0xd:  # LC_ID_DYLIB
                    found_id_cmd = True
                    # Read the string offset
                    if f.tell() + 4 > file_size:
                        break
                        
                    string_offset_bytes = f.read(4)
                    if len(string_offset_bytes) < 4:
                        break
                        
                    string_offset = struct.unpack('<I', string_offset_bytes)[0]
                    string_pos = pos + string_offset
                    
                    if string_pos >= file_size:
                        print(f"    {Colors.YELLOW}Warning: String offset beyond file bounds{Colors.RESET}")
                        return False

                    # Read current install ID to get its length
                    f.seek(string_pos)
                    current_id = b''
                    while True:
                        if f.tell() >= file_size:
                            break
                        byte = f.read(1)
                        if not byte or byte == b'\x00':
                            break
                        current_id += byte

                    # Calculate available space (from string start to end of load command)
                    available_space = cmdsize - string_offset
                    new_id_bytes = expected_id.encode('utf-8')

                    if len(new_id_bytes) + 1 > available_space:
                        print(f"    {Colors.YELLOW}Warning: New ID too long ({len(new_id_bytes)} bytes vs {available_space} available){Colors.RESET}")
                        return False

                    # Write new install ID
                    f.seek(string_pos)
                    f.write(new_id_bytes)
                    f.write(b'\x00')
                    # Pad with zeros if shorter than original
                    if len(new_id_bytes) < len(current_id):
                        f.write(b'\x00' * (len(current_id) - len(new_id_bytes)))

                    return True

                # Move to next load command
                f.seek(pos + cmdsize)

            if not found_id_cmd:
                print(f"    {Colors.YELLOW}Debug: LC_ID_DYLIB command not found (searched {ncmds} commands){Colors.RESET}")
            return False
    except Exception as e:
        print(f"    {Colors.YELLOW}Warning: Could not fix install ID: {e}{Colors.RESET}")
        return False

def fix_dylib_links(dylib_path):
    """
    Fix links to dependent libraries to use @rpath using macholib
    """
    try:
        # List of library names to fix (matching CMakeLists.txt)
        libraries_to_fix = [
            'libusb-1.0.0',
            'libopencv_highgui',
            'libopencv_videoio',
            'libopencv_imgcodecs',
            'libopencv_imgproc',
            'libopencv_core'
        ]
        
        # Library commands as defined in macholib
        LIBRARY_COMMANDS = [mach_o.LC_LOAD_DYLIB, mach_o.LC_LOAD_WEAK_DYLIB, 
                           mach_o.LC_REEXPORT_DYLIB, mach_o.LC_LOAD_UPWARD_DYLIB, 
                           mach_o.LC_LAZY_LOAD_DYLIB]
        
        macho = MachO.MachO(dylib_path)
        changed = False
        
        for header in macho.headers:
            for idx, (lc, cmd, data) in enumerate(header.commands):
                if lc.cmd not in LIBRARY_COMMANDS:
                    continue
                
                # Get the current library name
                current_link = data.decode('utf-8').rstrip('\x00')
                
                # Check if this link needs fixing
                for lib_name in libraries_to_fix:
                    if lib_name in current_link:
                        expected_link = f"@rpath/{lib_name}.dylib"
                        if current_link != expected_link:
                            header.rewriteDataForCommand(idx, expected_link.encode('utf-8'))
                            print(f"    {Colors.GREEN}Fixed link: {current_link} -> {expected_link}{Colors.RESET}")
                            changed = True
                        break
        
        if changed:
            with open(dylib_path, 'r+b') as f:
                macho.write(f)
        
        return changed
        
    except Exception as e:
        print(f"    {Colors.YELLOW}Warning: Could not fix links: {e}{Colors.RESET}")
        return False

def fix_dylib_links_raw(dylib_path):
    """
    DEPRECATED: Fix links to dependent libraries using raw binary manipulation.
    This function is kept for reference but is no longer used.
    Use fix_dylib_links() instead, which calls install_name_tool.
    """
    try:
        # List of library names to fix (matching CMakeLists.txt)
        libraries_to_fix = [
            'libusb-1.0.0',
            'libopencv_highgui',
            'libopencv_videoio',
            'libopencv_imgcodecs',
            'libopencv_imgproc',
            'libopencv_core'
        ]
        
        modified = False
        
        with open(dylib_path, 'r+b') as f:
            # Get file size
            f.seek(0, 2)
            file_size = f.tell()
            f.seek(0)
            
            # Read magic number
            magic_bytes = f.read(4)
            if len(magic_bytes) < 4:
                return False
                
            magic = struct.unpack('<I', magic_bytes)[0]
            
            # Handle universal/fat binaries
            # Note: Fat binary headers are ALWAYS big endian, regardless of magic (0xcafebabe or 0xbebafeca)
            arch_offset = 0
            if magic == 0xcafebabe or magic == 0xbebafeca:
                f.seek(0)
                f.read(4)  # Skip magic
                nfat_arch = struct.unpack('>I', f.read(4))[0]
                f.read(8)  # Skip cpu type and subtype (always big endian)
                arch_offset = struct.unpack('>I', f.read(4))[0]
                
                if arch_offset >= file_size:
                    return False
                    
                f.seek(arch_offset)
                magic_bytes = f.read(4)
                if len(magic_bytes) < 4:
                    return False
                magic = struct.unpack('<I', magic_bytes)[0]

            # Determine if 64-bit
            is_64bit = magic in (0xcffaedfe, 0xfeedfacf, 0xfeedface)
            
            # Calculate header size
            header_size = 32 if is_64bit else 28
            
            if arch_offset + header_size > file_size:
                return False

            # Read header
            f.seek(arch_offset)
            f.read(4)  # Skip magic
            f.read(4)  # Skip cputype
            f.read(4)  # Skip cpusubtype
            f.read(4)  # Skip filetype
            ncmds = struct.unpack('<I', f.read(4))[0]
            sizeofcmds = struct.unpack('<I', f.read(4))[0]
            
            if arch_offset + header_size + sizeofcmds > file_size:
                return False

            # Go to start of load commands
            f.seek(arch_offset + header_size)
            
            # Process each load command
            for _ in range(ncmds):
                pos = f.tell()
                if pos + 8 > file_size:
                    break
                    
                cmd_bytes = f.read(4)
                cmdsize_bytes = f.read(4)
                
                if len(cmd_bytes) < 4 or len(cmdsize_bytes) < 4:
                    break
                    
                cmd = struct.unpack('<I', cmd_bytes)[0]
                cmdsize = struct.unpack('<I', cmdsize_bytes)[0]
                
                if cmdsize < 8 or pos + cmdsize > file_size:
                    break

                # Check if this is LC_LOAD_DYLIB (0xc) or LC_LOAD_WEAK_DYLIB (0x18)
                if cmd in (0xc, 0x18):
                    # Read the string offset
                    if f.tell() + 12 > file_size:
                        break
                        
                    string_offset_bytes = f.read(4)
                    if len(string_offset_bytes) < 4:
                        break
                        
                    string_offset = struct.unpack('<I', string_offset_bytes)[0]
                    string_pos = pos + string_offset
                    
                    if string_pos >= file_size:
                        break

                    # Read current link path
                    f.seek(string_pos)
                    current_link = b''
                    while f.tell() < file_size:
                        byte = f.read(1)
                        if not byte or byte == b'\x00':
                            break
                        current_link += byte
                    
                    try:
                        current_link_str = current_link.decode('utf-8')
                    except:
                        f.seek(pos + cmdsize)
                        continue
                    
                    # Check if this link needs fixing
                    new_link = None
                    for lib_name in libraries_to_fix:
                        if lib_name in current_link_str:
                            expected_link = f"@rpath/{lib_name}.dylib"
                            if current_link_str != expected_link:
                                new_link = expected_link
                            break
                    
                    if new_link:
                        # Calculate available space
                        available_space = cmdsize - string_offset
                        new_link_bytes = new_link.encode('utf-8')
                        
                        if len(new_link_bytes) + 1 <= available_space:
                            # Write new link
                            f.seek(string_pos)
                            f.write(new_link_bytes)
                            f.write(b'\x00')
                            # Pad with zeros if shorter
                            if len(new_link_bytes) < len(current_link):
                                f.write(b'\x00' * (len(current_link) - len(new_link_bytes)))
                            modified = True
                            print(f"    {Colors.GREEN}Fixed link: {current_link_str} -> {new_link}{Colors.RESET}")

                # Move to next load command
                f.seek(pos + cmdsize)

        return modified
        
    except Exception as e:
        print(f"    {Colors.YELLOW}Warning: Could not fix links: {e}{Colors.RESET}")
        return False

def process_dylib(dylib_path, dylib_name, fix=False):
    """Process a single dylib file"""
    problems = check_dylib(dylib_path, dylib_name)

    if not problems:
        return True, []

    # Check if we should skip this file
    skip_types = ["SKIP", "COMPRESSED", "EMPTY"]
    if any(p[0] in skip_types for p in problems):
        return "SKIP", problems

    # Check if it's just informational (stub libraries)
    if all(p[0] == "INFO" for p in problems):
        return "INFO", problems

    if not fix:
        return False, problems

    # Attempt fixes
    fixed = []
    for problem_type, description in problems:
        if problem_type == "ID":
            if fix_dylib_id(dylib_path, dylib_name):
                fixed.append(("ID", description))
        elif problem_type == "LINK":
            if fix_dylib_links(dylib_path):
                fixed.append(("LINK", description))

    # Re-check after fixes
    remaining_problems = check_dylib(dylib_path, dylib_name)
    # Filter out INFO messages from remaining problems
    remaining_problems = [p for p in remaining_problems if p[0] != "INFO"]
    return len(remaining_problems) == 0, remaining_problems if remaining_problems else fixed

def find_mac_directories(root_path):
    """Find all mac, mac_x64, and mac_arm64 directories in indi-3rdparty"""
    mac_dirs = []

    for dirpath, dirnames, filenames in os.walk(root_path):
        for dirname in dirnames:
            if dirname in ['mac', 'mac_x64', 'mac_arm64', 'macos', 'macos_x64', 'macos_arm64']:
                full_path = os.path.join(dirpath, dirname)
                mac_dirs.append(full_path)

    return sorted(mac_dirs)

def find_dylibs_in_directory(directory):
    """Find all .bin and .dylib files (macOS libraries) in a directory"""
    dylibs = []
    for file in os.listdir(directory):
        if file.endswith('.bin') or file.endswith('.dylib'):
            full_path = os.path.join(directory, file)
            if os.path.isfile(full_path):
                # Don't filter here - let check_dylib determine if it's valid
                dylibs.append(full_path)
    return sorted(dylibs)

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='Scan and fix macOS dylibs (.bin files) in INDI 3rd party repository',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Scan only (no changes)
  python fix_macos_libs.py /path/to/indi-3rdparty

  # Scan and fix issues
  python fix_macos_libs.py /path/to/indi-3rdparty --fix

  # Scan specific directory
  python fix_macos_libs.py /path/to/indi-3rdparty/indi-qhy/mac_x64 --verbose
        '''
    )

    parser.add_argument('path', help='Path to indi-3rdparty root or specific mac directory')
    parser.add_argument('--fix', action='store_true', help='Apply fixes (default is check only)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show details for OK libraries too')
    parser.add_argument('--diagnose', action='store_true', help='Show detailed file format information')

    args = parser.parse_args()

    if not os.path.exists(args.path):
        print(f"{Colors.RED}Error: Path does not exist: {args.path}{Colors.RESET}")
        sys.exit(1)

    # Determine if we're looking at a single mac directory or scanning recursively
    path_basename = os.path.basename(args.path)
    if path_basename in ['mac', 'mac_x64', 'mac_arm64', 'macos', 'macos_x64', 'macos_arm64']:
        mac_dirs = [args.path]
        print(f"{Colors.BLUE}Scanning single directory: {args.path}{Colors.RESET}\n")
    else:
        print(f"{Colors.BLUE}Scanning for macOS library directories...{Colors.RESET}")
        mac_dirs = find_mac_directories(args.path)
        print(f"Found {len(mac_dirs)} macOS directories\n")

    if not mac_dirs:
        print(f"{Colors.YELLOW}No macOS library directories found{Colors.RESET}")
        sys.exit(0)

    total_libs = 0
    total_ok = 0
    total_fixed = 0
    total_issues = 0
    total_skipped = 0

    for mac_dir in mac_dirs:
        dylibs = find_dylibs_in_directory(mac_dir)

        if not dylibs:
            continue

        # Get driver name from parent directory
        driver_name = os.path.basename(os.path.dirname(mac_dir))
        arch = os.path.basename(mac_dir)

        print(f"{Colors.BOLD}{driver_name} ({arch}){Colors.RESET}")
        print(f"  Directory: {mac_dir}")

        for dylib_path in dylibs:
            total_libs += 1
            filename = os.path.basename(dylib_path)
            # Extract library name from .bin file
            dylib_name = get_library_name_from_file(filename)

            # Show file format if diagnosing
            if args.diagnose:
                file_format, format_desc = detect_file_format(dylib_path)
                file_size = os.path.getsize(dylib_path)
                print(f"  [{file_format}] {filename} ({file_size} bytes)")
                print(f"      {format_desc}")
                continue

            result, details = process_dylib(dylib_path, dylib_name, fix=args.fix)

            if result == True:
                total_ok += 1
                if args.verbose:
                    print(f"  {Colors.GREEN}✓{Colors.RESET} {filename} ({dylib_name})")
                    if details:  # Show what was fixed
                        for detail_type, description in details:
                            print(f"    {Colors.GREEN}Fixed {detail_type}: {description}{Colors.RESET}")
            elif result == "INFO":
                total_ok += 1  # Count as OK since it's informational only
                if args.verbose:
                    print(f"  {Colors.BLUE}ℹ{Colors.RESET} {filename} ({dylib_name})")
                    for problem_type, description in details:
                        print(f"    {Colors.BLUE}{description}{Colors.RESET}")
            elif result == "SKIP":
                total_skipped += 1
                print(f"  {Colors.BLUE}⊘{Colors.RESET} {filename} ({dylib_name})")
                for problem_type, description in details:
                    print(f"    {Colors.BLUE}{description}{Colors.RESET}")
            else:
                total_issues += 1
                print(f"  {Colors.RED}✗{Colors.RESET} {filename} ({dylib_name})")
                for problem_type, description in details:
                    if problem_type == "ERROR":
                        print(f"    {Colors.RED}{description}{Colors.RESET}")
                    elif problem_type == "WARNING":
                        print(f"    {Colors.YELLOW}{description}{Colors.RESET}")
                    elif problem_type == "INFO":
                        print(f"    {Colors.BLUE}{description}{Colors.RESET}")
                    else:
                        print(f"    {Colors.YELLOW}{problem_type}: {description}{Colors.RESET}")

        print()

    # Skip summary in diagnose mode
    if args.diagnose:
        sys.exit(0)

    # Summary
    print(f"{Colors.BOLD}Summary:{Colors.RESET}")
    print(f"  Total libraries: {total_libs}")
    print(f"  {Colors.GREEN}OK: {total_ok}{Colors.RESET}")
    if total_skipped > 0:
        print(f"  {Colors.BLUE}Skipped: {total_skipped}{Colors.RESET}")
    if args.fix:
        print(f"  {Colors.YELLOW}Issues remaining: {total_issues}{Colors.RESET}")
    else:
        print(f"  {Colors.YELLOW}Issues found: {total_issues}{Colors.RESET}")
        if total_issues > 0:
            print(f"\n{Colors.BLUE}Run with --fix to attempt automatic fixes{Colors.RESET}")

    sys.exit(0 if total_issues == 0 else 1)

if __name__ == "__main__":
    main()
