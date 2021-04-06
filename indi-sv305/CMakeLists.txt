cmake_minimum_required(VERSION 3.0)
PROJECT(indi_sv305_ccd CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

include(GNUInstallDirs)

set (SV305_VERSION_MAJOR 1)
set (SV305_VERSION_MINOR 2)
set (SV305_VERSION_PATCH 3)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SV305 REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_sv305_ccd.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_sv305_ccd.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

############# SVBONY SV305 CCD ###############
set(sv305ccd_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/sv305_ccd.cpp
)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(indi_sv305_ccd ${sv305ccd_SRCS})

IF("${CMAKE_SYSTEM}" MATCHES "Linux")
    target_link_libraries(indi_sv305_ccd ${SV305_LIBRARIES} ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} m ${ZLIB_LIBRARY})
ELSE("${CMAKE_SYSTEM}" MATCHES "Linux")
    message(FATAL_ERROR "Driver only available on Linux.")
ENDIF("${CMAKE_SYSTEM}" MATCHES "Linux")


install(TARGETS indi_sv305_ccd RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_sv305_ccd.xml DESTINATION ${INDI_DATA_DIR})
