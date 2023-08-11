cmake_minimum_required (VERSION 3.0)
project (libplayerone)

# Using PlayerOne Camera SDK Version 3.4.1 updated on 2023-7-24
# Using PlayerOne EFW    SDK Version 1.2.0 updated on 2023-3-30

set (POACAM_VERSION "3.4.1")
set (POACAM_SOVERSION "3")

set (POAEFW_VERSION "1.2.0")
set (POAEFW_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (PlayerOneCamera  SHARED IMPORTED)
add_library (PlayerOnePW      SHARED IMPORTED)

set_target_properties (PlayerOneCamera  PROPERTIES VERSION ${POACAM_VERSION} SOVERSION ${POACAM_SOVERSION})
set_target_properties (PlayerOnePW      PROPERTIES VERSION ${POAEFW_VERSION} SOVERSION ${POAEFW_SOVERSION})

if (APPLE)

  set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "mac/libPlayerOneCamera.bin")
  set_property (TARGET PlayerOnePW      PROPERTY IMPORTED_LOCATION "mac/libPlayerOnePW.bin")
    
  FIX_MACOS_LIBRARIES("libPlayerOneCamera" "mac/libPlayerOneCamera.bin" "PLAYER ONE")
  FIX_MACOS_LIBRARIES("libPlayerOnePW"     "mac/libPlayerOnePW.bin" "PLAYER ONE")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "armv6/libPlayerOneCamera.bin")
    set_property (TARGET PlayerOnePW      PROPERTY IMPORTED_LOCATION "armv6/libPlayerOnePW.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "armv8/libPlayerOneCamera.bin")
    set_property (TARGET PlayerOnePW      PROPERTY IMPORTED_LOCATION "armv8/libPlayerOnePW.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "x64/libPlayerOneCamera.bin")
    set_property (TARGET PlayerOnePW      PROPERTY IMPORTED_LOCATION "x64/libPlayerOnePW.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    #x86 library is not provided from manufacturer
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "")
    set_property (TARGET PlayerOnePW      PROPERTY IMPORTED_LOCATION "")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-player_one_astronomy.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (
  FILES PlayerOneCamera.h PlayerOnePW.h ConvFuncs.h
  DESTINATION include/libplayerone
)

# Install library
install_imported (
  TARGETS PlayerOneCamera PlayerOnePW
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
