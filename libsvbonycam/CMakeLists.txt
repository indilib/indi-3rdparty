cmake_minimum_required(VERSION 3.16)
project (libsvbonycam)

set (LIBSVBONYCAM_VERSION "57.27348")
set (LIBSVBONYCAM_SOVERSION "57")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (svbonycam SHARED IMPORTED)

set_target_properties (svbonycam PROPERTIES VERSION ${LIBSVBONYCAM_VERSION} SOVERSION ${LIBSVBONYCAM_SOVERSION})

if (APPLE)

  set_property (TARGET svbonycam PROPERTY IMPORTED_LOCATION "mac/libsvbonycam.bin")

  FIX_MACOS_LIBRARIES("libsvbonycam" "mac/libsvbonycam.bin" "SVBONYCAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET svbonycam PROPERTY IMPORTED_LOCATION "armhf/libsvbonycam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET svbonycam PROPERTY IMPORTED_LOCATION "arm64/libsvbonycam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET svbonycam PROPERTY IMPORTED_LOCATION "x64/libsvbonycam.bin")
  else ()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-svbonycam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES svbonycam.h DESTINATION include/libsvbonycam)

# Install library
install_imported (TARGETS svbonycam DESTINATION ${CMAKE_INSTALL_LIBDIR})
