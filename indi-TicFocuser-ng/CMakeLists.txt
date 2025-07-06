if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

PROJECT(TicFocuserNG CXX)

set (TICFOCUSER_VERSION_MAJOR 1)
set (TICFOCUSER_VERSION_MINOR 1)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)

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

find_package(LIBUSB_1)
if (LIBUSB_1_FOUND)

    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/LibUsbConnection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicUsb.cpp
        )

    include_directories(${LIBUSB_1_INCLUDE_DIRS})
    target_link_libraries( indi_ticfocuser-ng "${LIBUSB_1_LIBRARIES}")

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

pkg_check_modules(LIBTIC-1 libpololu-tic-1)
if(LIBTIC-1_FOUND)

    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/PololuUsbInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/PololuUsbConnection.cpp
        )
    include_directories(${LIBTIC-1_INCLUDE_DIR})
    target_compile_options(indi_ticfocuser-ng PUBLIC ${LIBTIC-1_CFLAGS})
    target_link_libraries( indi_ticfocuser-ng "${LIBTIC-1_LIBRARIES}")

    set(WITH_LIBTIC TRUE)
endif()

target_sources(indi_ticfocuser-ng PUBLIC ${INDI_TICFOCUSER-NG_SRCS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/TicFocuser_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_ticfocuser-ng.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser-ng.xml)

install(TARGETS indi_ticfocuser-ng RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser-ng.xml DESTINATION ${INDI_DATA_DIR})


