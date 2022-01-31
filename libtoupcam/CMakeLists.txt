cmake_minimum_required (VERSION 3.0)
project (libtoupcam)

# Using ToupCamera SDK Version: 50.19728.20211022
set (LIBTOUPCAM_VERSION "1.50.1")
set (LIBTOUPCAM_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (toupcam SHARED IMPORTED)

set_target_properties (toupcam PROPERTIES VERSION ${LIBTOUPCAM_VERSION} SOVERSION ${LIBTOUPCAM_SOVERSION})

if (APPLE)

  set_property (TARGET toupcam PROPERTY IMPORTED_LOCATION "mac/libtoupcam.bin")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET toupcam PROPERTY IMPORTED_LOCATION "armhf/libtoupcam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET toupcam PROPERTY IMPORTED_LOCATION "arm64/libtoupcam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET toupcam PROPERTY IMPORTED_LOCATION "x64/libtoupcam.bin")
  else ()
    set_property (TARGET toupcam PROPERTY IMPORTED_LOCATION "x86/libtoupcam.bin")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-toupcam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES toupcam.h DESTINATION include/libtoupcam)

# Install library
install_imported (TARGETS toupcam DESTINATION ${CMAKE_INSTALL_LIBDIR})
