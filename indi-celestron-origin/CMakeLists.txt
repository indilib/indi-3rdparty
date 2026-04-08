cmake_minimum_required(VERSION 3.16)
project(indi-celestron-origin CXX)

include(GNUInstallDirs)

# These two lines are key to the 3rdparty build environment:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

# Core INDI dependencies
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(TIFF REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Nova REQUIRED)

# Qt autodetection (same as official pattern)
find_package(Qt6 COMPONENTS Core Network Gui QUIET)
if (Qt6_FOUND)
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Network)
    message(STATUS "Using Qt6")
else()
    find_package(Qt5 COMPONENTS Core Network Gui REQUIRED)
    set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Network)
    message(STATUS "Using Qt5")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_definitions(-Wall -Wextra -Wpedantic -DINDI_DISABLE_LEGACY)

# Includes
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${INDI_INCLUDE_DIR}
)

# Source files
add_executable(indi_celestron_origin
    indi_origin.cpp
    OriginBackendSimple.cpp
    SimpleWebSocket.cpp
    TelescopeDataProcessor.cpp
)

target_link_libraries(indi_celestron_origin
    ${QT_LIBRARIES}
    ${INDI_LIBRARIES}
    ${NOVA_LIBRARIES}
    TIFF::TIFF
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Installation
install(TARGETS indi_celestron_origin RUNTIME DESTINATION bin)
install(FILES indi_celestron_origin.xml DESTINATION ${INDI_DATA_DIR})

