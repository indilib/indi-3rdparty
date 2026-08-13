# ScopeLink INDI driver - build
#
# Copyright (C) 2026 Astrolabs Hungary Kft.
#
# Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
# Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
#
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.

cmake_minimum_required(VERSION 3.16)
PROJECT(indi_scopelink CXX C)

# Both paths are appended so that this builds either as a subdirectory of an indi-3rdparty checkout or
# on its own during development, the second entry being a copy of the same modules placed beside it.
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")

set(SCOPELINK_VERSION_MAJOR 1)
set(SCOPELINK_VERSION_MINOR 0)

find_package(INDI 2.0.0 REQUIRED)

include(GNUInstallDirs)

# The repository's shared build settings - C++17, position independent code, the warning set and the
# hardening flags - rather than restating any of them here.
include(CMakeCommon)

# ---------------------------------------------------------------------------------------------------

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)
# The driver list XML carries no licence header, unlike every other file here. KStars does not read that
# file with an XML parser that tolerates comments: with a comment in it the driver is silently missing
# from the device list, which was measured on KStars 3.8.2 rather than guessed at.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_scopelink.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_scopelink.xml @ONLY)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/indi)
include_directories(${INDI_INCLUDE_DIR})

# The protocol core is built once and linked into the driver and both tools. It is kept separate from
# the driver because it is the part that has to agree, byte for byte, with the vendor's Windows driver,
# and that agreement is only testable if it can be built without libindi.
set(SCOPELINK_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/parameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/faults.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libscopelink/src/simulator.cpp
)

add_library(scopelinkcore STATIC ${SCOPELINK_CORE_SOURCES})

# ---------------------------------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------------------------------

add_executable(indi_scopelink
    ${CMAKE_CURRENT_SOURCE_DIR}/indi/scopelink_driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indi/scopelink_diagnostics.cpp
)

target_link_libraries(indi_scopelink scopelinkcore ${INDI_LIBRARIES})

install(TARGETS indi_scopelink RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_scopelink.xml DESTINATION ${INDI_DATA_DIR})

# Without this rule the controller comes up owned by root on Linux. It goes under /usr/lib rather than
# /etc because /etc/udev is where the administrator's own overrides live, and a package has no business
# writing there. Linux only: macOS has no udev, and installing there means creating a directory the
# system will never read, which is what libtoupcam and libsvbony avoid the same way.
option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)

if(INDI_INSTALL_UDEV_RULES AND UNIX AND NOT APPLE)
  if(NOT DEFINED UDEVRULES_INSTALL_DIR)
    set(UDEVRULES_INSTALL_DIR "/usr/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  endif()
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/udev/99-scopelink.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()

# ---------------------------------------------------------------------------------------------------
# Support tools
# ---------------------------------------------------------------------------------------------------

add_executable(scopelink-cli ${CMAKE_CURRENT_SOURCE_DIR}/tools/scopelink_cli.cpp)
target_link_libraries(scopelink-cli scopelinkcore)

add_executable(scopelink-simulator ${CMAKE_CURRENT_SOURCE_DIR}/tools/scopelink_simulator.cpp)
target_link_libraries(scopelink-simulator scopelinkcore)

install(TARGETS scopelink-cli scopelink-simulator RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------------------------------
# Manual pages
# ---------------------------------------------------------------------------------------------------

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/man/indi_scopelink.1
    ${CMAKE_CURRENT_SOURCE_DIR}/man/scopelink-cli.1
    ${CMAKE_CURRENT_SOURCE_DIR}/man/scopelink-simulator.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# ---------------------------------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------------------------------

# Only when this is the project being built. add_test() needs enable_testing() from the top of the
# tree, and calling it here would reach into a parent project that did not ask for it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()

    add_executable(test_scopelink ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scopelink.cpp)
    target_link_libraries(test_scopelink scopelinkcore)

    # The recorded vectors are read from the source tree rather than copied next to the binary, so that a
    # capture added by hand is picked up by the next run without a reconfigure.
    target_compile_definitions(test_scopelink
                               PRIVATE SCOPELINK_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")

    add_test(NAME scopelink COMMAND test_scopelink)
endif()
