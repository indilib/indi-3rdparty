PROJECT(indi_weewx_json C CXX)
cmake_minimum_required(VERSION 3.0)

set (WEEWX_VERSION_MAJOR 1)
set (WEEWX_VERSION_MINOR 0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

set(CMAKE_CXX_FLAGS "-g -std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(INDI REQUIRED)
find_package(CURL REQUIRED)

if (CMAKE_VERSION VERSION_LESS 3.12.0)
set(CURL ${CURL_LIBRARIES})
else()
set(CURL CURL::libcurl)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_weewx_json.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_weewx_json.xml)
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CURL_INCLUDE_DIR})

include(CMakeCommon)

set(weewx_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_weewx_json.cpp
)

add_executable(indi_weewx_json ${weewx_SRCS})

target_link_libraries(indi_weewx_json ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${CURL})

install(TARGETS indi_weewx_json RUNTIME DESTINATION bin )

install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_weewx_json.xml DESTINATION ${INDI_DATA_DIR})
