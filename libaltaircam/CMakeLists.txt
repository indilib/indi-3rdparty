cmake_minimum_required (VERSION 3.0)
project (libaltaircam)

set (LIBALTAIRCAM_VERSION "54.22587")
set (LIBALTAIRCAM_SOVERSION "54")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (altaircam SHARED IMPORTED)

set_target_properties (altaircam PROPERTIES VERSION ${LIBALTAIRCAM_VERSION} SOVERSION ${LIBALTAIRCAM_SOVERSION})

if (APPLE)

  set_property (TARGET altaircam PROPERTY IMPORTED_LOCATION "mac/libaltaircam.bin")

  FIX_MACOS_LIBRARIES("libaltaircam" "mac/libaltaircam.bin" "ALTAIR")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET altaircam PROPERTY IMPORTED_LOCATION "armhf/libaltaircam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET altaircam PROPERTY IMPORTED_LOCATION "arm64/libaltaircam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET altaircam PROPERTY IMPORTED_LOCATION "x64/libaltaircam.bin")
  else()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-altaircam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES altaircam.h DESTINATION include/libaltaircam)

# Install library
install_imported (TARGETS altaircam DESTINATION ${CMAKE_INSTALL_LIBDIR})
