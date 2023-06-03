cmake_minimum_required (VERSION 3.0)
project (libastroasis)

set (OASISFOCUSER_VERSION "1.0.5")
set (OASISFOCUSER_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (oasisfocuser SHARED IMPORTED)

set_target_properties (oasisfocuser PROPERTIES VERSION ${OASISFOCUSER_VERSION} SOVERSION ${OASISFOCUSER_SOVERSION})

if (APPLE)

  set_property (TARGET oasisfocuser  PROPERTY IMPORTED_LOCATION "mac/liboasisfocuser.bin")

  FIX_MACOS_LIBRARIES("liboasisfocuser" "mac/liboasisfocuser.bin" "ASTROASIS")
  
  # Install library
  install_imported (TARGETS oasisfocuser DESTINATION ${CMAKE_INSTALL_LIBDIR})

elseif (UNIX AND NOT WIN32)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET oasisfocuser PROPERTY IMPORTED_LOCATION "armhf/liboasisfocuser.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET oasisfocuser PROPERTY IMPORTED_LOCATION "arm64/liboasisfocuser.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET oasisfocuser PROPERTY IMPORTED_LOCATION "x64/liboasisfocuser.bin")
  else ()
    message (FATAL_ERROR "x86-32 architecture is not supported.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-astroasis.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

  # Install library
  install_imported (TARGETS oasisfocuser DESTINATION ${CMAKE_INSTALL_LIBDIR})

endif ()

# Install header files
install (FILES AOFocus.h DESTINATION include/libastroasis)
