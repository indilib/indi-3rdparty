cmake_minimum_required (VERSION 3.0)
project (libplayerone)

# Using PlayerOne Camera SDK Version 3.0.2 updated on 2022-6-14

set (POACAM_VERSION "3.0.2")
set (POACAM_SOVERSION "3")


list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (PlayerOneCamera  SHARED IMPORTED)

set_target_properties (PlayerOneCamera  PROPERTIES VERSION ${POACAM_VERSION} SOVERSION ${POACAM_SOVERSION})

if (APPLE)

  set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "mac/libPlayerOneCamera.bin")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "armv6/libPlayerOneCamera.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "armv8/libPlayerOneCamera.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "x64/libPlayerOneCamera.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    #x86 library is not provided from manufacturer
    set_property (TARGET PlayerOneCamera  PROPERTY IMPORTED_LOCATION "")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-player_one_astronomy.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (
  FILES PlayerOneCamera.h
  DESTINATION include/libplayerone
)

# Install library
install_imported (
  TARGETS PlayerOneCamera
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
