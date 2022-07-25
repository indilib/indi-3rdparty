PROJECT(indi_nut C CXX)
cmake_minimum_required(VERSION 3.0)

set (NUT_VERSION_MAJOR 0)
set (NUT_VERSION_MINOR 1)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

set(CMAKE_CXX_FLAGS "-g -std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(INDI REQUIRED)
find_package(NUTClient)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_nut.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_nut.xml)
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NUTCLIENT_INCLUDE_DIR})

include(CMakeCommon)

set(nut_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_nut.cpp)

add_executable(indi_nut ${nut_SRCS})

target_link_libraries(indi_nut ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NUTCLIENT_LIBRARIES})

install(TARGETS indi_nut RUNTIME DESTINATION bin )

install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_nut.xml DESTINATION ${INDI_DATA_DIR})
