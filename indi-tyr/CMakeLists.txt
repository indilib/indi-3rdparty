# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Prometheus Astro
cmake_minimum_required(VERSION 3.16)
project(indi_tyr VERSION 0.3.0 LANGUAGES CXX)

include(CTest)

include(GNUInstallDirs)
# Use the upstream module when built inside indi-3rdparty. Keep the standalone
# build usable with packaged INDI development headers and pkg-config.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules")
find_package(INDI 2.2.5 QUIET COMPONENTS driver)
set(TYR_USE_PKG_CONFIG OFF)
if(NOT INDI_FOUND)
    set(TYR_USE_PKG_CONFIG ON)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(INDI REQUIRED IMPORTED_TARGET libindi>=2.2.5)
    find_library(INDI_DRIVER_LIBRARY indidriver)
    if(NOT INDI_DRIVER_LIBRARY)
        message(FATAL_ERROR "Tyr requires the INDI driver library (indidriver)")
    endif()
endif()
find_package(nlohmann_json 3.2 REQUIRED)

add_executable(indi_tyr_telescope
    tyr_telescope.cpp
    tyr_configuration.cpp
)

target_compile_features(indi_tyr_telescope PRIVATE cxx_std_17)
target_compile_options(indi_tyr_telescope PRIVATE -Wall -Wextra -Wpedantic)
if(TYR_USE_PKG_CONFIG)
    target_link_libraries(indi_tyr_telescope PRIVATE PkgConfig::INDI ${INDI_DRIVER_LIBRARY})
else()
    target_include_directories(indi_tyr_telescope SYSTEM PRIVATE ${INDI_INCLUDE_DIR})
    target_link_libraries(indi_tyr_telescope PRIVATE ${INDI_LIBRARIES})
endif()
target_link_libraries(indi_tyr_telescope PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(indi_tyr_telescope PRIVATE
    TYR_DRIVER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    TYR_DRIVER_VERSION_MINOR=${PROJECT_VERSION_MINOR})

configure_file(indi_tyr.xml.in indi_tyr.xml @ONLY)

install(TARGETS indi_tyr_telescope RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_tyr.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/indi)
install(FILES COPYING README.md THIRD_PARTY.md doc/index.md
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/indi-tyr)
install(FILES doc/indi_tyr_telescope.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(TYR_BASH bash)
    find_program(TYR_INDISERVER indiserver)
    find_program(TYR_INDI_GETPROP indi_getprop)
    find_program(TYR_INDI_SETPROP indi_setprop)
    foreach(tool TYR_BASH TYR_INDISERVER TYR_INDI_GETPROP TYR_INDI_SETPROP)
        if(NOT ${tool})
            message(FATAL_ERROR "Tyr tests require ${tool}; install indi-bin/bash or disable BUILD_TESTING")
        endif()
    endforeach()
    add_test(NAME tyr_indi_simulation
        COMMAND ${TYR_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_simulation.sh $<TARGET_FILE:indi_tyr_telescope>)
    set_tests_properties(tyr_indi_simulation PROPERTIES TIMEOUT 30)
    add_test(NAME tyr_indi_protocol
        COMMAND ${TYR_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_protocol.sh $<TARGET_FILE:indi_tyr_telescope>)
    set_tests_properties(tyr_indi_protocol PROPERTIES TIMEOUT 30)
    add_test(NAME tyr_indi_configuration
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/configuration_protocol.py $<TARGET_FILE:indi_tyr_telescope>)
    set_tests_properties(tyr_indi_configuration PROPERTIES TIMEOUT 60)
    add_test(NAME tyr_indi_tcp
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/configuration_protocol.py $<TARGET_FILE:indi_tyr_telescope> tcp)
    set_tests_properties(tyr_indi_tcp PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
set(CPACK_PACKAGE_NAME "indi-tyr")
set(CPACK_PACKAGE_VENDOR "Prometheus Astro")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native INDI telescope driver for the Prometheus Astro Tyr mount")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://prometheusastro.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Prometheus Astro")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "indi-bin, libindi1 (>= 2.2.5)")
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_GENERATOR DEB)
include(CPack)
endif()
