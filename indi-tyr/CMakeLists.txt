# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Prometheus Astro
cmake_minimum_required(VERSION 3.16)
PROJECT(indi-tyr C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

include(GNUInstallDirs)

set(TYR_VERSION_MAJOR 0)
set(TYR_VERSION_MINOR 3)

find_package(INDI 2.2.5 COMPONENTS driver REQUIRED)
find_package(Nova REQUIRED)

if(INDI_JSONLIB)
    set(JSONLIB "")
    message(STATUS "Using indi bundled json library")
else(INDI_JSONLIB)
    find_package(nlohmann_json REQUIRED)
    add_definitions(-D_USE_SYSTEM_JSONLIB)
    set(JSONLIB nlohmann_json::nlohmann_json)
    message(STATUS "Using system provided Niels Lohmann's json library")
endif(INDI_JSONLIB)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_tyr.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_tyr.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

include(CMakeCommon)

########### Tyr Telescope ###########
set(indi_tyr_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/tyr_telescope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tyr_configuration.cpp
)

add_executable(indi_tyr_telescope ${indi_tyr_SRCS})
target_link_libraries(indi_tyr_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${JSONLIB})

install(TARGETS indi_tyr_telescope RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_tyr.xml DESTINATION ${INDI_DATA_DIR})
install(FILES COPYING THIRD_PARTY.md DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/indi-tyr)

########### Tests ###########
if(INDI_BUILD_UNITTESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    enable_testing()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(TYR_BASH bash)
    find_program(TYR_INDISERVER indiserver)
    find_program(TYR_INDI_GETPROP indi_getprop)
    find_program(TYR_INDI_SETPROP indi_setprop)
    foreach(tool TYR_BASH TYR_INDISERVER TYR_INDI_GETPROP TYR_INDI_SETPROP)
        if(NOT ${tool})
            message(FATAL_ERROR "Tyr tests require ${tool}; install indi-bin/bash or disable INDI_BUILD_UNITTESTS")
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
