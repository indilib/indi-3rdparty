cmake_minimum_required(VERSION 3.16)
PROJECT(indi-orion_ssg3 CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

IF(APPLE)
ELSE(APPLE)
set(RULES_INSTALL_DIR "/lib/udev/rules.d")
ENDIF()
set (ORION_SSG3_VERSION_MAJOR 0)
set (ORION_SSG3_VERSION_MINOR 1)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_orion_ssg3.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_orion_ssg3.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${USB1_INCLUDE_DIR})

include(CMakeCommon)

# This warning only valid for Clang above version 3.9
IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 3.8.9)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-undefined-var-template")
ENDIF ()

########### DSI ###########
set(indiorionssg3_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/orion_ssg3.c
   ${CMAKE_CURRENT_SOURCE_DIR}/orion_ssg3_ccd.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indiorionssg3 indiorionssg3_SRCS 10 cpp)
ENDIF ()

add_executable(indi_orion_ssg3_ccd ${indiorionssg3_SRCS})

target_link_libraries(indi_orion_ssg3_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${USB1_LIBRARIES} )

install(TARGETS indi_orion_ssg3_ccd RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_orion_ssg3.xml DESTINATION ${INDI_DATA_DIR})

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES 99-orionssg3.rules DESTINATION ${RULES_INSTALL_DIR})
ENDIF()

