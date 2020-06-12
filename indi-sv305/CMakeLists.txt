cmake_minimum_required(VERSION 2.8)
PROJECT(sv305_ccd CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

include(GNUInstallDirs)

set (SV305_VERSION_MAJOR 1)
set (SV305_VERSION_MINOR 0)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

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

add_executable(indi_sv305_ccd ${sv305ccd_SRCS})

target_link_libraries(indi_sv305_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} m ${ZLIB_LIBRARY})

install(TARGETS indi_sv305_ccd RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_sv305_ccd.xml DESTINATION ${INDI_DATA_DIR})
