cmake_minimum_required(VERSION 3.16)
project (libbressercam)

option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)

set (LIBBRESSERCAM_VERSION "59.29176")
set (LIBBRESSERCAM_SOVERSION "59")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include(CMakeCommon)
include (InstallImported)

add_library (bressercam SHARED IMPORTED)

set_target_properties (bressercam PROPERTIES VERSION ${LIBBRESSERCAM_VERSION} SOVERSION ${LIBBRESSERCAM_SOVERSION})

if (APPLE)

  set_property (TARGET bressercam PROPERTY IMPORTED_LOCATION "mac/libbressercam.bin")

  FIX_MACOS_LIBRARIES("libbressercam" "mac/libbressercam.bin" "BRESSERCAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET bressercam PROPERTY IMPORTED_LOCATION "armhf/libbressercam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET bressercam PROPERTY IMPORTED_LOCATION "arm64/libbressercam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET bressercam PROPERTY IMPORTED_LOCATION "x64/libbressercam.bin")
  else ()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  if (INDI_INSTALL_UDEV_RULES)
    set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
    install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-bressercam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
  endif()

endif ()

# Install header files
install (FILES bressercam.h DESTINATION include/libbressercam)

# Install library
install_imported (TARGETS bressercam DESTINATION ${CMAKE_INSTALL_LIBDIR})
