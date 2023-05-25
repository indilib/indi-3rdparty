cmake_minimum_required (VERSION 3.0)
project (libnncam)

set (LIBNNCAM_VERSION "54.22587")
set (LIBNNCAM_SOVERSION "54")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (nncam SHARED IMPORTED)

set_target_properties (nncam PROPERTIES VERSION ${LIBNNCAM_VERSION} SOVERSION ${LIBNNCAM_SOVERSION})

if (APPLE)

  set_property (TARGET nncam PROPERTY IMPORTED_LOCATION "mac/libnncam.bin")

  FIX_MACOS_LIBRARIES("libnncam" "mac/libnncam.bin" "NNCAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET nncam PROPERTY IMPORTED_LOCATION "armhf/libnncam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET nncam PROPERTY IMPORTED_LOCATION "arm64/libnncam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET nncam PROPERTY IMPORTED_LOCATION "x64/libnncam.bin")
  else ()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-nncam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES nncam.h DESTINATION include/libnncam)

# Install library
install_imported (TARGETS nncam DESTINATION ${CMAKE_INSTALL_LIBDIR})
