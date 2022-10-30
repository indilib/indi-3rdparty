cmake_minimum_required(VERSION 3.0)
PROJECT(indi_Excalibur C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set (DREAMFOCUSER_VERSION_MAJOR 2)
set (DREAMFOCUSER_VERSION_MINOR 1)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_Excalibur.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_Excalibur.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

include(CMakeCommon)

########### RBFocus Excalibur ###########
set(indiExcalibur_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/Excalibur.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indiExcalibur indiExcalibur_SRCS 10 cpp)
ENDIF ()

add_executable(indi_Excalibur ${indiExcalibur_SRCS})

target_link_libraries(indi_Excalibur ${INDI_DRIVER_LIBRARIES} pthread)

install(TARGETS indi_Excalibur RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_Excalibur.xml DESTINATION ${INDI_DATA_DIR})
