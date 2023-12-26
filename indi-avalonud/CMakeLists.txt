cmake_minimum_required(VERSION 3.16)
project(indi-avalonud CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

include(GNUInstallDirs)

set (AVALONUD_VERSION_MAJOR 2)
set (AVALONUD_VERSION_MINOR 2)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
find_package(ZMQ REQUIRED)

if(INDI_JSONLIB)
    set(JSONLIB "")
    message(STATUS "Using indi bundled json library")
else(INDI_JSONLIB)
    find_package(nlohmann_json REQUIRED)
    add_definitions(-D_USE_SYSTEM_JSONLIB)
    set(JSONLIB nlohmann_json::nlohmann_json)
    message(STATUS "Using system provided Niels Lohmann's json library")
endif(INDI_JSONLIB)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_avalonud.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_avalonud.xml
)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${INDI_INCLUDE_DIR} )
include_directories( ${Threads_INCLUDE_DIR} )
include_directories( ${ZMQ_INCLUDE_DIR} )

include(CMakeCommon)

########### AVALONUD TELESCOPE ###########

add_executable(
    indi_avalonud_telescope
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_avalonud_telescope.cpp
)
target_link_libraries(indi_avalonud_telescope ${INDI_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ZMQ_LIBRARIES} ${JSONLIB})

install(TARGETS indi_avalonud_telescope RUNTIME DESTINATION bin)

########### AVALONUD FOCUSER ###########

add_executable(
    indi_avalonud_focuser
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_avalonud_focuser.cpp
)
target_link_libraries(indi_avalonud_focuser ${INDI_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ZMQ_LIBRARIES} ${JSONLIB})

install(TARGETS indi_avalonud_focuser RUNTIME DESTINATION bin)

########### AVALONUD AUX ###########

add_executable(
    indi_avalonud_aux
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_avalonud_aux.cpp
)
target_link_libraries(indi_avalonud_aux ${INDI_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ZMQ_LIBRARIES} ${JSONLIB})

install(TARGETS indi_avalonud_aux RUNTIME DESTINATION bin)

####################################

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_avalonud.xml DESTINATION ${INDI_DATA_DIR})
