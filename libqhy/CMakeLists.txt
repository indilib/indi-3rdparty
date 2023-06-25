cmake_minimum_required (VERSION 3.0)
project (libqhy)

# QHY SDK 23.05.12
set (LIBQHY_VERSION "23.5.12")
set (LIBQHY_SOVERSION "23")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (qhyccd SHARED IMPORTED)

set_target_properties (qhyccd PROPERTIES VERSION ${LIBQHY_VERSION} SOVERSION ${LIBQHY_SOVERSION})

if (APPLE)

  set (QHY_FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/qhy/firmware" CACHE STRING "QHY firmware installation directory")

  set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd.dylib")

  FIX_MACOS_LIBRARIES("libqhyccd" "libqhyccd.dylib" "QHY")

elseif (UNIX AND NOT WIN32)

  set (QHY_FIRMWARE_INSTALL_DIR "/lib/firmware/qhy" CACHE STRING "QHY firmware installation directory")

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd_armv6.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd_armv8.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd_x86_64.bin")
  else ()
    message (FATAL_ERROR "x86-32 architecture is not supported.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 85-qhyccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES qhyccd.h qhyccderr.h qhyccdcamdef.h qhyccdstruct.h DESTINATION include/libqhy)

# Install firmware
install (
   CODE "
   file(GLOB QHY_FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*) \n
   file(INSTALL DESTINATION ${QHY_FIRMWARE_INSTALL_DIR} TYPE FILE FILES \${QHY_FIRMWARE})"
)

# Install library
install_imported (TARGETS qhyccd DESTINATION ${CMAKE_INSTALL_LIBDIR})
