cmake_minimum_required(VERSION 3.16)
project (libastroasis)

set (OASIS_FOCUSER_VERSION "2.0.2")
set (OASIS_FOCUSER_SOVERSION "2")

set (OASIS_FILTER_WHEEL_VERSION "1.2.1")
set (OASIS_FILTER_WHEEL_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (oasisfocuser     SHARED IMPORTED)
add_library (oasisfilterwheel SHARED IMPORTED)

set_target_properties (oasisfocuser     PROPERTIES VERSION ${OASIS_FOCUSER_VERSION} SOVERSION ${OASIS_FOCUSER_SOVERSION})
set_target_properties (oasisfilterwheel PROPERTIES VERSION ${OASIS_FILTER_WHEEL_VERSION} SOVERSION ${OASIS_FILTER_WHEEL_SOVERSION})

if (APPLE)

  set_property (TARGET oasisfocuser     PROPERTY IMPORTED_LOCATION "mac/liboasisfocuser.bin")
  set_property (TARGET oasisfilterwheel PROPERTY IMPORTED_LOCATION "mac/liboasisfilterwheel.bin")

  FIX_MACOS_LIBRARIES("liboasisfocuser"     "mac/liboasisfocuser.bin" "ASTROASIS")
  FIX_MACOS_LIBRARIES("liboasisfilterwheel" "mac/liboasisfilterwheel.bin" "ASTROASIS")

elseif (UNIX AND NOT WIN32)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET oasisfocuser     PROPERTY IMPORTED_LOCATION "armhf/liboasisfocuser.bin")
    set_property (TARGET oasisfilterwheel PROPERTY IMPORTED_LOCATION "armhf/liboasisfilterwheel.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET oasisfocuser     PROPERTY IMPORTED_LOCATION "arm64/liboasisfocuser.bin")
    set_property (TARGET oasisfilterwheel PROPERTY IMPORTED_LOCATION "arm64/liboasisfilterwheel.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET oasisfocuser     PROPERTY IMPORTED_LOCATION "x64/liboasisfocuser.bin")
    set_property (TARGET oasisfilterwheel PROPERTY IMPORTED_LOCATION "x64/liboasisfilterwheel.bin")
  else ()
    message (FATAL_ERROR "x86-32 architecture is not supported.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-astroasis.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install library
install_imported (TARGETS oasisfocuser     DESTINATION ${CMAKE_INSTALL_LIBDIR})
install_imported (TARGETS oasisfilterwheel DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install header files
install (FILES AOFocus.h          DESTINATION include/libastroasis)
install (FILES OasisFilterWheel.h DESTINATION include/libastroasis)
