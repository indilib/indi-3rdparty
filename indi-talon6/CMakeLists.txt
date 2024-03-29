cmake_minimum_required(VERSION 3.16)
PROJECT(indi_talon6 C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(INDI_TALON6_VERSION_MAJOR 2)
set(INDI_TALON6_VERSION_MINOR 0)

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_talon6.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_talon6.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

include(CMakeCommon)

########### Talon6  ###########
set(indi_talon6_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/talon6.cpp
   )

add_executable(indi_talon6 ${indi_talon6_SRCS})

target_link_libraries(indi_talon6 ${INDI_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_talon6 RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_talon6.xml DESTINATION ${INDI_DATA_DIR})
