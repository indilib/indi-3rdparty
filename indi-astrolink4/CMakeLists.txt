cmake_minimum_required(VERSION 3.2)
PROJECT(indi-astrolink4 CXX C)

set (ASTROLINK4_VERSION_MAJOR 0)
set (ASTROLINK4_VERSION_MINOR 1)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

SET(CMAKE_CXX_STANDARD 11)

find_package(INDI REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_astrolink4.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_astrolink4.xml )

################ AstroLink4 ################

set(indi_astrolink4_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indi_astrolink4.cpp
   )

add_executable(indi_astrolink4 ${indi_astrolink4_SRCS})
target_link_libraries(indi_astrolink4 ${INDI_LIBRARIES})

install(TARGETS indi_astrolink4 RUNTIME DESTINATION bin )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_astrolink4.xml DESTINATION ${INDI_DATA_DIR})

