cmake_minimum_required (VERSION 3.0)
project (libahp_gt)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(AHP_GT_VERSION_MAJOR 1)
set(AHP_GT_VERSION_MINOR 6)
set(AHP_GT_VERSION_RELEASE 3)
set(AHPGT_VERSION "${AHP_GT_VERSION_MAJOR}.${AHP_GT_VERSION_MINOR}.${AHP_GT_VERSION_RELEASE}")
set(AHPGT_SOVERSION ${AHP_GT_VERSION_MAJOR})

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (ahp_gt SHARED IMPORTED)

set_target_properties (ahp_gt PROPERTIES VERSION ${AHPGT_VERSION} SOVERSION ${AHPGT_SOVERSION})

if (UNIX AND NOT WIN32 AND NOT APPLE)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET ahp_gt PROPERTY IMPORTED_LOCATION "libahp_gt.so.armhf")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET ahp_gt PROPERTY IMPORTED_LOCATION "libahp_gt.so.arm64")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET ahp_gt PROPERTY IMPORTED_LOCATION "libahp_gt.so.amd64")
  else (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET ahp_gt PROPERTY IMPORTED_LOCATION "libahp_gt.so.i386")
  endif ()

endif ()

# Install header files
install (FILES ahp_gt.h DESTINATION include/ahp)

# Install library
install_imported (TARGETS ahp_gt DESTINATION ${CMAKE_INSTALL_LIBDIR})
