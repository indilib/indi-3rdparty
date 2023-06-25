cmake_minimum_required (VERSION 3.0)
project (libomegonprocam)

set (LIBOMEGONPROCAM_VERSION "54.22587")
set (LIBOMEGONPROCAM_SOVERSION "54")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (omegonprocam SHARED IMPORTED)

set_target_properties (omegonprocam PROPERTIES VERSION ${LIBOMEGONPROCAM_VERSION} SOVERSION ${LIBOMEGONPROCAM_SOVERSION})

if (APPLE)

  set_property (TARGET omegonprocam PROPERTY IMPORTED_LOCATION "mac/libomegonprocam.bin")

  FIX_MACOS_LIBRARIES("libomegonprocam" "mac/libomegonprocam.bin" "OMEGON")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET omegonprocam PROPERTY IMPORTED_LOCATION "armhf/libomegonprocam.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET omegonprocam PROPERTY IMPORTED_LOCATION "arm64/libomegonprocam.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET omegonprocam PROPERTY IMPORTED_LOCATION "x64/libomegonprocam.bin")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-omegonprocam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES omegonprocam.h DESTINATION include/libomegonprocam)

# Install library
install_imported (TARGETS omegonprocam DESTINATION ${CMAKE_INSTALL_LIBDIR})
