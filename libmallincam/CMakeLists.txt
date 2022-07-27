cmake_minimum_required (VERSION 3.0)
project (libmallincam)

# Using Version: 49.18608.20210313
set (LIBMALLINCAM_VERSION "1.49.1")
set (LIBMALLINCAM_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (mallincam SHARED IMPORTED)

set_target_properties (mallincam PROPERTIES VERSION ${LIBMALLINCAM_VERSION} SOVERSION ${LIBMALLINCAM_SOVERSION})

if (APPLE)

  set_property (TARGET mallincam PROPERTY IMPORTED_LOCATION "mac/libmallincam.bin")

  FIX_LIBRARY_ID("libmallincam" "mac/libmallincam.bin" "MALLINCAM")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET mallincam PROPERTY IMPORTED_LOCATION "armhf/libmallincam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET mallincam PROPERTY IMPORTED_LOCATION "arm64/libmallincam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET mallincam PROPERTY IMPORTED_LOCATION "x64/libmallincam.bin")
  else ()
    set_property (TARGET mallincam PROPERTY IMPORTED_LOCATION "x86/libmallincam.bin")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-mallincam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES mallincam.h DESTINATION include/libmallincam)

# Install library
install_imported (TARGETS mallincam DESTINATION ${CMAKE_INSTALL_LIBDIR})

