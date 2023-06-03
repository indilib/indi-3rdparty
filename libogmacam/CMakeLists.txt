cmake_minimum_required (VERSION 3.0)
project (libogmacam)

set (LIBOGMACAM_VERSION "54.22587")
set (LIBOGMACAM_SOVERSION "54")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (ogmacam SHARED IMPORTED)

set_target_properties (ogmacam PROPERTIES VERSION ${LIBOGMACAM_VERSION} SOVERSION ${LIBOGMACAM_SOVERSION})

if (APPLE)

  set_property (TARGET ogmacam PROPERTY IMPORTED_LOCATION "mac/libogmacam.bin")

  FIX_MACOS_LIBRARIES("libogmacam" "mac/libogmacam.bin" "OGMACAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET ogmacam PROPERTY IMPORTED_LOCATION "armhf/libogmacam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET ogmacam PROPERTY IMPORTED_LOCATION "arm64/libogmacam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET ogmacam PROPERTY IMPORTED_LOCATION "x64/libogmacam.bin")
  else ()
    message (FATAL_ERROR "unsupported architecture.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-ogmacam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES ogmacam.h DESTINATION include/libogmacam)

# Install library
install_imported (TARGETS ogmacam DESTINATION ${CMAKE_INSTALL_LIBDIR})
