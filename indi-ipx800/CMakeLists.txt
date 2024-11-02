cmake_minimum_required(VERSION 3.13)
PROJECT(indi_ipx800 C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
##LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

include(GNUInstallDirs)

set(IPX800_VERSION_MAJOR 1)
set(IPX800_VERSION_MINOR 0)

find_package(INDI REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

include(CMakeCommon)

########### IPX800  ###########
set(indi_ipx800_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_ipx800.cpp
   )

add_executable(indi_ipx800 ${indi_ipx800_SRCS})

target_link_libraries(indi_ipx800 ${INDI_LIBRARIES})

install(TARGETS indi_ipx800 RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/indi_ipx800.xml DESTINATION ${INDI_DATA_DIR})
