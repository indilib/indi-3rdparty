cmake_minimum_required(VERSION 3.16)
project (libatik)

# Built with SDK API 2024.11.26_2038
set (ATIK_VERSION "2.2.22")
set (ATIK_SOVERSION "2")

set (FLYCAPTURE_VERSION "2.13.3.31")
set (FLYCAPTURE_SOVERSION "2")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (atikcameras SHARED IMPORTED)

set_target_properties (atikcameras PROPERTIES VERSION ${ATIK_VERSION} SOVERSION ${ATIK_SOVERSION})

if (APPLE)

  set_property (TARGET atikcameras  PROPERTY IMPORTED_LOCATION "mac/libatikcameras.bin")

  FIX_MACOS_LIBRARIES("libatikcameras" "mac/libatikcameras.bin" "ATIK")
  
  # Install library
  install_imported (TARGETS atikcameras DESTINATION ${CMAKE_INSTALL_LIBDIR})

elseif (UNIX AND NOT WIN32)
  add_library (flycapture  SHARED IMPORTED)
  set_target_properties (flycapture  PROPERTIES VERSION ${FLYCAPTURE_VERSION} SOVERSION ${FLYCAPTURE_SOVERSION})

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET atikcameras PROPERTY IMPORTED_LOCATION "armhf/libatikcameras.bin")
    set_property (TARGET flycapture  PROPERTY IMPORTED_LOCATION "armhf/libflycapture.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET atikcameras PROPERTY IMPORTED_LOCATION "arm64/libatikcameras.bin")
    set_property (TARGET flycapture  PROPERTY IMPORTED_LOCATION "arm64/libflycapture.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET atikcameras PROPERTY IMPORTED_LOCATION "x64/libatikcameras.bin")
    set_property (TARGET flycapture  PROPERTY IMPORTED_LOCATION "x64/libflycapture.bin")
  else ()
    set_property (TARGET atikcameras PROPERTY IMPORTED_LOCATION "x86/libatikcameras.bin")
    set_property (TARGET flycapture  PROPERTY IMPORTED_LOCATION "x86/libflycapture.bin")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-atik.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

  # Install library
  install_imported (TARGETS atikcameras flycapture DESTINATION ${CMAKE_INSTALL_LIBDIR})

endif ()

# Install header files
install (FILES AtikCameras.h AtikDefs.h DESTINATION include/libatik)
