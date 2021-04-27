cmake_minimum_required (VERSION 3.0)
project (libsv305)

set (SVBCAMERASDK_VERSION "1.6.5")
set (SVBCAMERASDK_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (SVBCameraSDK SHARED IMPORTED)

set_target_properties (SVBCameraSDK PROPERTIES VERSION ${SVBCAMERASDK_VERSION} SOVERSION ${SVBCAMERASDK_SOVERSION})

if (APPLE)
    message (FATAL_ERROR "MacOS is not supported")
endif (APPLE)

if (UNIX AND NOT WIN32)


    if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
        set_property (TARGET SVBCameraSDK PROPERTY IMPORTED_LOCATION "libSVBCameraSDK_armv6.bin")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set_property (TARGET SVBCameraSDK PROPERTY IMPORTED_LOCATION "libSVBCameraSDK_armv8.bin")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set_property (TARGET SVBCameraSDK PROPERTY IMPORTED_LOCATION "libSVBCameraSDK_amd64.bin")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
        set_property (TARGET SVBCameraSDK PROPERTY IMPORTED_LOCATION "libSVBCameraSDK_x86.bin")
    endif ()

    # Install udev rules
    set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
    install (FILES 90-svbonyusb.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

    # install header files
    install (FILES SVBCameraSDK.h DESTINATION include/libsv305)

    # Install library
    install_imported (TARGETS SVBCameraSDK DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()
