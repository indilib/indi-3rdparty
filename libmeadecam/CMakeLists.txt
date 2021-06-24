cmake_minimum_required (VERSION 3.0)
project (libmeadecam)

# Using SDK Version: 30.13011.20180926
set (LIBMEADECAM_VERSION "1.30.0")
set (LIBMEADECAM_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (meadecam SHARED IMPORTED)

set_target_properties (meadecam PROPERTIES VERSION ${LIBMEADECAM_VERSION} SOVERSION ${LIBMEADECAM_SOVERSION})

if (APPLE)

  set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "mac/libmeadecam.bin")

elseif (UNIX AND NOT WIN32)

  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(x86_64|amd64)")
    set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "x64/libmeadecam.bin")
  elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^i[3-6]86")
    set_property (TARGET meadecam PROPERTY IMPORTED_LOCATION "x86/libmeadecam.bin")
  else ()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-meadecam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES meadecam.h DESTINATION include/libmeadecam)

# Install library
install_imported (TARGETS meadecam DESTINATION ${CMAKE_INSTALL_LIBDIR})

