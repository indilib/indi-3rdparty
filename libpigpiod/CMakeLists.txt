cmake_minimum_required(VERSION 3.0)
PROJECT(libpigpiod CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 2)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
find_package(RT REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

include(CMakeCommon)

set(GPIO_LIBRARIES "pigpiod_if2.so")

add_compile_options(-Wall)

# library pigpio.(so|a)
add_library(pigpio OBJECT pigpio.c command.c custom.cext)
target_compile_options(pigpio PRIVATE -Wno-type-limits -Wno-sign-compare -Wno-maybe-uninitialized -Wno-unused-parameter)

# pigpiod daemon for controlling GPIO
add_executable(pigpiod pigpiod.c)
target_link_libraries(pigpiod pigpio RT::RT Threads::Threads)

# Install pigpiod
install(TARGETS pigpiod RUNTIME DESTINATION bin )
install( FILES pigpio.h DESTINATION include)

# Install pigpiod systemd service
include(FindPkgConfig)
pkg_check_modules(SYSTEMD "systemd")
if (SYSTEMD_FOUND AND "${SYSTEMD_SERVICES_INSTALL_DIR}" STREQUAL "")
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
        --variable=systemdsystemunitdir systemd
        OUTPUT_VARIABLE SYSTEMD_SERVICES_INSTALL_DIR)
    string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SERVICES_INSTALL_DIR
        "${SYSTEMD_SERVICES_INSTALL_DIR}")
        
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pigpiod.service.in
            ${CMAKE_CURRENT_BINARY_DIR}/pigpiod.service
            @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pigpiod.service DESTINATION
            ${SYSTEMD_SERVICES_INSTALL_DIR} COMPONENT cp)
### install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_BINARY_DIR}/install_pigpiod.sh)")

elseif (NOT SYSTEMD_FOUND AND SYSTEMD_SERVICES_INSTALL_DIR)
    message (FATAL_ERROR "Variable SYSTEMD_SERVICES_INSTALL_DIR is\
        defined, but we can't find systemd using pkg-config")
endif()

if (SYSTEMD_FOUND)
    set(WITH_SYSTEMD "ON")
    message(STATUS "systemd services install dir: ${SYSTEMD_SERVICES_INSTALL_DIR}")
else()
    set(WITH_SYSTEMD "OFF")
endif (SYSTEMD_FOUND)
