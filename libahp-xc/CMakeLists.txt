cmake_minimum_required (VERSION 3.0)
project (libahp_xc NONE)

set (AHPXC_VERSION "1.0.12")
set (AHPXC_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (ahp_xc SHARED IMPORTED)

set_target_properties (ahp_xc PROPERTIES VERSION ${AHPXC_VERSION} SOVERSION ${AHPXC_SOVERSION})

if (UNIX AND NOT WIN32 AND NOT APPLE)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET ahp_xc PROPERTY IMPORTED_LOCATION "libahp_xc.so.armhf")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET ahp_xc PROPERTY IMPORTED_LOCATION "libahp_xc.so.arm64")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET ahp_xc PROPERTY IMPORTED_LOCATION "libahp_xc.so.amd64")
  else (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET ahp_xc PROPERTY IMPORTED_LOCATION "libahp_xc.so.i386")
  endif ()

endif ()

# Install header files
install (FILES ahp_xc.h DESTINATION include/ahp)

# Install library
install_imported (TARGETS ahp_xc DESTINATION lib)
