cmake_minimum_required (VERSION 3.0)
project (libtscam)

set (LIBTSCAM_VERSION "54.22587")
set (LIBTSCAM_SOVERSION "54")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (tscam SHARED IMPORTED)

set_target_properties (tscam PROPERTIES VERSION ${LIBTSCAM_VERSION} SOVERSION ${LIBTSCAM_SOVERSION})

if (APPLE)

  set_property (TARGET tscam PROPERTY IMPORTED_LOCATION "mac/libtscam.bin")

  FIX_MACOS_LIBRARIES("libtscam" "mac/libtscam.bin" "TSCAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET tscam PROPERTY IMPORTED_LOCATION "armhf/libtscam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET tscam PROPERTY IMPORTED_LOCATION "arm64/libtscam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET tscam PROPERTY IMPORTED_LOCATION "x64/libtscam.bin")
  else ()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-tscam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES tscam.h DESTINATION include/libtscam)

# Install library
install_imported (TARGETS tscam DESTINATION ${CMAKE_INSTALL_LIBDIR})
