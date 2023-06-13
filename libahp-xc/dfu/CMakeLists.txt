cmake_minimum_required (VERSION 3.0)
project (libdfu)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(DFU_VERSION_MAJOR 1)
set(DFU_VERSION_MINOR 0)
set(DFU_VERSION_RELEASE 0)
set(AHPXC_VERSION "${DFU_VERSION_MAJOR}.${DFU_VERSION_MINOR}.${DFU_VERSION_RELEASE}")
set(AHPXC_SOVERSION ${DFU_VERSION_MAJOR})

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (dfu SHARED IMPORTED)

set_target_properties (dfu PROPERTIES VERSION ${AHPXC_VERSION} SOVERSION ${AHPXC_SOVERSION})

if (UNIX AND NOT WIN32 AND NOT APPLE)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET dfu PROPERTY IMPORTED_LOCATION "libDFU.so.armhf")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET dfu PROPERTY IMPORTED_LOCATION "libDFU.so.arm64")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET dfu PROPERTY IMPORTED_LOCATION "libDFU.so.amd64")
  else (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET dfu PROPERTY IMPORTED_LOCATION "libDFU.so.i386")
  endif ()

endif ()

# Install header files
install (FILES dfu_file.h DESTINATION include/dfu)
install (FILES dfu.h DESTINATION include/dfu)
install (FILES dfu_load.h DESTINATION include/dfu)
install (FILES dfuse.h DESTINATION include/dfu)
install (FILES dfuse_mem.h DESTINATION include/dfu)
install (FILES dfu_util.h DESTINATION include/dfu)
install (FILES libdfu.h DESTINATION include/dfu)
install (FILES portable.h DESTINATION include/dfu)
install (FILES quirks.h DESTINATION include/dfu)
install (FILES usb_dfu.h DESTINATION include/dfu)

# Install library
install_imported (TARGETS dfu DESTINATION ${CMAKE_INSTALL_LIBDIR})
