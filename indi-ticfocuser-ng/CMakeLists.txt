cmake_minimum_required(VERSION 3.16)
PROJECT(TicFocuserNG CXX C)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set (TICFOCUSER_VERSION_MAJOR 1)
set (TICFOCUSER_VERSION_MINOR 1)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)

find_package(INDI REQUIRED)

set(INDI_TICFOCUSER-NG_CORE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection/UsbConnectionBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection/SerialConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/TiclibInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicDefs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/StreamSerial.cpp
)

add_executable(indi_ticfocuser-ng ${INDI_TICFOCUSER-NG_CORE_SRCS})
target_link_libraries( indi_ticfocuser-ng ${INDI_LIBRARIES})

target_include_directories(indi_ticfocuser-ng PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${INDI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pololu-tic-software/include
)

pkg_check_modules(LIBUSB1_PC libusb-1.0 REQUIRED)
message(STATUS "Found Libusb-1.0 via pkg-config: Includes=${LIBUSB1_PC_INCLUDE_DIRS} Libraries=${LIBUSB1_PC_LIBRARIES}")

target_include_directories(indi_ticfocuser-ng PUBLIC ${LIBUSB1_PC_INCLUDE_DIRS})
target_link_libraries(indi_ticfocuser-ng ${LIBUSB1_PC_LIBRARIES})

pkg_check_modules(LIBUSBP_PC libusbp)

if (NOT LIBUSBP_PC_FOUND)
    message(STATUS "LIBUSBP-1 (libusbp) not found on system via pkg-config. Trying to build from bundled libusbp.")

    set(LIBUSBP_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusbp")
    if(EXISTS "${LIBUSBP_ROOT_DIR}/CMakeLists.txt")
        message(STATUS "Found bundled libusbp at '${LIBUSBP_ROOT_DIR}'.")

        add_subdirectory("${LIBUSBP_ROOT_DIR}" libusbp_build_dir)

        if(TARGET usbp)
            set(LIBUSBP_PC_LIBRARIES usbp)
            set(LIBUSBP_PC_INCLUDE_DIRS "${LIBUSBP_ROOT_DIR}/include")
            set(LIBUSBP_PC_FOUND TRUE) # Marque comme trouvé
            message(STATUS "Successfully configured using bundled libusbp library 'usbp'.")
        else()
            message(WARNING "Bundled libusbp found, but 'usbp' target not created. Please check its CMakeLists.txt and libusbp/src/CMakeLists.txt.")
        endif()
    else()
        message(WARNING "Bundled libusbp not found at '${LIBUSBP_ROOT_DIR}' or it does not contain a CMakeLists.txt.")
    endif()
endif()

if (LIBUSBP_PC_FOUND)
    message(STATUS "Using LIBUSBP-1 (libusbp) (from system or bundled source).")
    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/LibUsbConnection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicUsb.cpp
    )

    include_directories(${LIBUSBP_PC_INCLUDE_DIRS})
    target_link_libraries(indi_ticfocuser-ng "${LIBUSBP_PC_LIBRARIES}")

    set(WITH_LIBUSBP TRUE)
else()
    message(WARNING "LIBUSBP-1 (libusbp) was not found on the system and could not be built from bundled source. USB functionality might be limited.")
endif()

find_package(USB1)
if (USB1_FOUND AND NOT WITH_LIBUSBP)
    message(STATUS "Using other USB1 library found on system.")
    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/LibUsbConnection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicUsb.cpp
    )
    include_directories(${USB1_INCLUDE_DIRS})
    target_link_libraries( indi_ticfocuser-ng ${USB1_LIBRARY})
    set(WITH_LIBUSB TRUE)
elseif(USB1_FOUND AND WITH_LIBUSBP)
    message(STATUS "USB1 found, but LIBUSBP-1 already configured. Skipping USB1 to avoid conflicts.")
endif()

find_package(LibBluetooth)
if (LIBBLUETOOTH_FOUND)
    message(STATUS "LibBluetooth found. Enabling Bluetooth support.")

    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/StreamBT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/BluetoothConnection.cpp
    )

    target_link_libraries( indi_ticfocuser-ng "${LIBBLUETOOTH_LIBRARIES}")

    set(WITH_BLUETOOTH TRUE)
else()
    message(STATUS "LibBluetooth not found. Bluetooth support will be disabled.")
endif()

set(POLOLU_TIC_SOFTWARE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pololu-tic-software")
if(EXISTS "${POLOLU_TIC_SOFTWARE_ROOT_DIR}/CMakeLists.txt")
    message(STATUS "Found bundled pololu-tic-software at '${POLOLU_TIC_SOFTWARE_ROOT_DIR}'. Attempting to build and link.")
    add_subdirectory("${POLOLU_TIC_SOFTWARE_ROOT_DIR}" pololu_tic_software_build_dir)

    if(TARGET lib)
        message(STATUS "Successfully configured using bundled pololu-tic-software library 'lib'.")
        list(APPEND INDI_TICFOCUSER-NG_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/PololuUsbInterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/connection/PololuUsbConnection.cpp
        )
        target_link_libraries(indi_ticfocuser-ng lib) # Link directly to the CMake target
        target_include_directories(indi_ticfocuser-ng PUBLIC "${POLOLU_TIC_SOFTWARE_ROOT_DIR}/include")
        set(WITH_LIBTIC TRUE)
    else()
        message(WARNING "Bundled pololu-tic-software found, but 'lib' target not created. Falling back to system search.")
    endif()
endif()

if(NOT WITH_LIBTIC)
    pkg_check_modules(LIBTIC-1 libpololu-tic-1)
    if(LIBTIC-1_FOUND)
        message(STATUS "Using LIBTIC-1 (libpololu-tic-1) from system.")
        list(APPEND INDI_TICFOCUSER-NG_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/PololuUsbInterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/connection/PololuUsbConnection.cpp
        )
        if(LIBTIC-1_INCLUDE_DIR)
            target_include_directories(indi_ticfocuser-ng PUBLIC ${LIBTIC-1_INCLUDE_DIR})
        endif()
        target_link_libraries(indi_ticfocuser-ng "${LIBTIC-1_LIBRARIES}")
        set(WITH_LIBTIC TRUE)
    else()
        message(WARNING "LIBTIC-1 (libpololu-tic-1) was not found on the system and could not be built from bundled source. Functionality might be limited.")
    endif()
endif()

target_sources(indi_ticfocuser-ng PUBLIC ${INDI_TICFOCUSER-NG_SRCS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/TicFocuser_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_ticfocuser-ng.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser-ng.xml)

install(TARGETS indi_ticfocuser-ng RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser-ng.xml DESTINATION ${INDI_DATA_DIR})
