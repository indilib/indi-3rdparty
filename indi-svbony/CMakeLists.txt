cmake_minimum_required(VERSION 3.0)
PROJECT(indi_svbony_ccd CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

include(GNUInstallDirs)

set (SVBONY_VERSION_MAJOR 1)
set (SVBONY_VERSION_MINOR 3)
set (SVBONY_VERSION_PATCH 8)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SVBONY REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_svbony_ccd.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_svbony_ccd.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

############# SVBONY SVBONY CCD ###############
set(svbonyccd_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/svbony_ccd.cpp
)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(indi_svbony_ccd ${svbonyccd_SRCS})

IF(WIN32)
    message(FATAL_ERROR "Driver only available on Linux.")
ELSE()
    target_link_libraries(indi_svbony_ccd ${SVBONY_LIBRARIES} ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} m ${ZLIB_LIBRARY})
ENDIF()


install(TARGETS indi_svbony_ccd RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_svbony_ccd.xml DESTINATION ${INDI_DATA_DIR})
