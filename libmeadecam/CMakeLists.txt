cmake_minimum_required(VERSION 3.16)
project (libmeadecam)

set (LIBMEADECAM_VERSION "57.28200")
set (LIBMEADECAM_SOVERSION "57")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (meadecam SHARED IMPORTED)

set_target_properties (meadecam PROPERTIES VERSION ${LIBMEADECAM_VERSION} SOVERSION ${LIBMEADECAM_SOVERSION})

if (APPLE)

  set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "mac/libmeadecam.bin")

  FIX_MACOS_LIBRARIES("libmeadecam" "mac/libmeadecam.bin" "MEADECAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "armhf/libmeadecam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "arm64/libmeadecam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "x64/libmeadecam.bin")
  else ()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-meadecam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES meadecam.h DESTINATION include/libmeadecam)

# Install library
install_imported (TARGETS meadecam DESTINATION ${CMAKE_INSTALL_LIBDIR})
