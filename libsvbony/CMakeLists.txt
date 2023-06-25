cmake_minimum_required (VERSION 3.0)
project (libsvbony)

set (SVBCAMERASDK_VERSION "1.11.4")
set (SVBCAMERASDK_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (SVBCameraSDK SHARED IMPORTED)

set_target_properties (SVBCameraSDK PROPERTIES VERSION ${SVBCAMERASDK_VERSION} SOVERSION ${SVBCAMERASDK_SOVERSION})

if(WIN32)

    message(FATAL_ERROR "MS Windows not supported.")

elseif (APPLE)

    set_property (TARGET SVBCameraSDK PROPERTY IMPORTED_LOCATION "libSVBCameraSDK_mac64.bin")

    FIX_MACOS_LIBRARIES("libSVBCameraSDK" "libSVBCameraSDK_mac64.bin" "SVBONY")

elseif (UNIX)

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

endif ()

# install header files
install (FILES SVBCameraSDK.h DESTINATION include/libsvbony)

# Install library
install_imported (TARGETS SVBCameraSDK DESTINATION ${CMAKE_INSTALL_LIBDIR})
