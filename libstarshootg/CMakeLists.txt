cmake_minimum_required(VERSION 3.16)
project (libstarshootg)

set (LIBSTARSHOOTG_VERSION "59.29176")
set (LIBSTARSHOOTG_SOVERSION "59")

option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include(CMakeCommon)
include (InstallImported)

add_library (starshootg SHARED IMPORTED)

set_target_properties (starshootg PROPERTIES VERSION ${LIBSTARSHOOTG_VERSION} SOVERSION ${LIBSTARSHOOTG_SOVERSION})

if (APPLE)

  set_property (TARGET starshootg PROPERTY IMPORTED_LOCATION "mac/libstarshootg.bin")

  FIX_MACOS_LIBRARIES("libstarshootg" "mac/libstarshootg.bin" "StarShoot")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET starshootg PROPERTY IMPORTED_LOCATION "armhf/libstarshootg.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET starshootg PROPERTY IMPORTED_LOCATION "arm64/libstarshootg.bin")
  elseif(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET starshootg PROPERTY IMPORTED_LOCATION "x64/libstarshootg.bin")
  endif ()

  # Install udev rules
  if (INDI_INSTALL_UDEV_RULES)
    set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
    install (FILES 99-starshootg.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
  endif ()

endif ()

# Install header files
install (FILES starshootg.h DESTINATION include/libstarshootg)

# Install library
install_imported (TARGETS starshootg DESTINATION ${CMAKE_INSTALL_LIBDIR})
