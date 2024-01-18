cmake_minimum_required(VERSION 3.16)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

PROJECT(TicFocuserNG CXX)

set (TICFOCUSER_VERSION_MAJOR 1)
set (TICFOCUSER_VERSION_MINOR 0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")


set(INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/UsbConnectionBase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/SerialConnection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/TiclibInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicBase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicDefs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/StreamSerial.cpp
    )

add_executable(indi_ticfocuser-ng)

find_package(INDI REQUIRED)
target_link_libraries( indi_ticfocuser-ng indidriver)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(
    {CMAKE_CURRENT_BINARY_DIR}
    ${INDI_INCLUDE_DIR} )

find_package(USB1)
if (USB1_FOUND)

    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/LibUsbConnection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicUsb.cpp
        )

    include_directories(${USB1_INCLUDE_DIRS})
    target_link_libraries( indi_ticfocuser-ng "${USB1_LIBRARIES}")

    set(WITH_LIBUSB TRUE)
endif()

find_package(LIBBLUETOOTH)
if (LIBBLUETOOTH_FOUND)

    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/StreamBT.cpp            ${CMAKE_CURRENT_SOURCE_DIR}/connection/BluetoothConnection.cpp
    )

    target_link_libraries( indi_ticfocuser-ng "${LIBBLUETOOTH_LIBRARIES}")

    set(WITH_BLUETOOTH TRUE)
endif()


find_package(LIBTIC1)
if(LIBTIC1_FOUND)

    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/PololuUsbInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/PololuUsbConnection.cpp
        )
    include_directories(${LIBTIC1_INCLUDE_DIR})
    target_compile_options(indi_ticfocuser-ng PUBLIC ${LIBTIC1_CFLAGS})
    target_link_libraries( indi_ticfocuser-ng "${LIBTIC1_LIBRARIES}")

    set(WITH_LIBTIC TRUE)
endif()

target_sources(indi_ticfocuser-ng PUBLIC ${INDI_TICFOCUSER-NG_SRCS})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/TicFocuser_config.h
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_ticfocuser.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser.xml
)

install(TARGETS indi_ticfocuser-ng RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser.xml DESTINATION ${INDI_DATA_DIR})

