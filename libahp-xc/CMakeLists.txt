cmake_minimum_required (VERSION 3.0)
project (libahp_xc)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(AHP_XC_VERSION_MAJOR 1)
set(AHP_XC_VERSION_MINOR 1)
set(AHP_XC_VERSION_RELEASE 2)
set(AHPXC_VERSION "${AHP_XC_VERSION_MAJOR}.${AHP_XC_VERSION_MINOR}.${AHP_XC_VERSION_RELEASE}")
set(AHPXC_SOVERSION ${AHP_XC_VERSION_MAJOR})

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (ahp_xc SHARED IMPORTED)

if (UNIX AND NOT WIN32 AND NOT APPLE)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set (AHP_XC_DEB "libahp-xc_${AHPXC_VERSION}_armhf.deb")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set (AHP_XC_DEB "libahp-xc_${AHPXC_VERSION}_arm64.deb")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set (AHP_XC_DEB "libahp-xc_${AHPXC_VERSION}_amd64.deb")
  else (CMAKE_SIZEOF_VOID_P MATCHES "4")
    set (AHP_XC_DEB "libahp-xc_${AHPXC_VERSION}_i386.deb")
  endif ()

endif ()

# Install library
execute_process (COMMAND sudo "dpkg -i ${AHP_XC_DEB}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
