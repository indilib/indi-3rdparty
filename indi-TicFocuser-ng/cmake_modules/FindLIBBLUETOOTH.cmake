# - Find libbluetooth
# Find the Bluez Linux Bluetooth library
#
#  LIBBLUETOOTH_INCLUDE_DIR - where to find bluetooth.h etc.
#  LIBBLUETOOTH_LIBRARIES    - List of libraries when using libbluetooth.
#  LIBBLUETOOTH_FOUND        - True if libbluetooth was found.


# [License]
# The Ariba-Underlay Copyright
#
# Copyright (c) 2008-2012, Institute of Telematics, UniversitÃ€t Karlsruhe (TH)
#
# Institute of Telematics
# UniversitÃ€t Karlsruhe (TH)
# Zirkel 2, 76128 Karlsruhe
# Germany
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE INSTITUTE OF TELEMATICS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INSTITUTE OF TELEMATICS OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation
# are those of the authors and should not be interpreted as representing
# official policies, either expressed or implied, of the Institute of
# Telematics.
# [License]


# Look for the header files.
find_path(LIBBLUETOOTH_INCLUDE_DIR NAMES bluetooth/bluetooth.h)
mark_as_advanced(LIBBLUETOOTH_INCLUDE_DIR)


# Look for the library.
find_library(LIBBLUETOOTH_LIBRARY NAMES bluetooth
    DOC "The path to the Bluez Linux Bluetooth library"
    )
mark_as_advanced(LIBBLUETOOTH_LIBRARY)


# handle the QUIETLY and REQUIRED arguments and set LIBBLUETOOTH_FOUND to TRUE
# if all listed variables are TRUE
find_package(PackageHandleStandardArgs)
find_package_handle_standard_args(LibBluetooth DEFAULT_MSG
    LIBBLUETOOTH_LIBRARY
    LIBBLUETOOTH_INCLUDE_DIR
    )

if(LIBBLUETOOTH_FOUND)
    set(LIBBLUETOOTH_LIBRARIES "${LIBBLUETOOTH_LIBRARY}")
endif()