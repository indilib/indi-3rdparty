cmake_minimum_required(VERSION 3.16)
PROJECT(indi_astromechfoc C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(INDI_ASTROMECHFOC_VERSION_MAJOR 0)
set(INDI_ASTROMECHFOC_VERSION_MINOR 2)

find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_astromechfoc.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_astromechfoc.xml )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

include(CMakeCommon)

########### Astromechanics Focuser ###########
set(indiastromechfoc_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/astromech_focuser.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indiastromechfoc indiastromechfoc_SRCS 10 cpp)
ENDIF ()

add_executable(indi_astromechfoc ${indiastromechfoc_SRCS})

target_link_libraries(indi_astromechfoc ${INDI_DRIVER_LIBRARIES} pthread)

install(TARGETS indi_astromechfoc RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_astromechfoc.xml DESTINATION ${INDI_DATA_DIR})


