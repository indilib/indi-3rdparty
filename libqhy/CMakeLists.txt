cmake_minimum_required (VERSION 3.0)
project (libqhy)

# QHY SDK 22.05.11
set (LIBQHY_VERSION "22.5.11")
set (LIBQHY_SOVERSION "22")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (qhyccd SHARED IMPORTED)

set_target_properties (qhyccd PROPERTIES VERSION ${LIBQHY_VERSION} SOVERSION ${LIBQHY_SOVERSION})

if (APPLE)

  set (FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/qhy/firmware" CACHE STRING "QHY firmware installation directory")

  set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd.dylib")

  # One problem that is often encountered on macs when using rpaths is that the ID the dylib has does not match the links being made to the dylib
  # Since when using RPATHS, programs link to libraries using their IDs, that would cause a problem later when a program being built tries to link to the wrong name for the dylib
  # This should get the current ID of the dylib and make sure it matches what we expect it to be, and stops the build if it does not.
  execute_process(COMMAND bash "-c" "otool -D ${CMAKE_CURRENT_SOURCE_DIR}/libqhyccd.dylib | tail -n 1 | tr -d '\n'" OUTPUT_VARIABLE DYLIB_ID)
  if (${DYLIB_ID} STREQUAL "@rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib")
    message (STATUS "The Mac dylib in the repo matches the libqhy SDK in the repo, linking should succeed.")
  else (${DYLIB_ID} STREQUAL "@rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib")
    if (${DYLIB_ID} MATCHES "rpath")
      message (FATAL_ERROR "The Mac dylib in the libqhy source folder does not match the current SDK Version.  Linking will fail since it uses rpaths for linking, since the library is called ${DYLIB_ID} and the linker thinks it should be called @rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib.  Please request that the library be updated in the repo.")
    else (${DYLIB_ID} MATCHES "rpath")
      message (WARNING "The Mac dylib does not have an RPATH ID, you might have issues linking it later since it probably uses an absoute path.  Please request that the ID be changed in the repo.")
    endif (${DYLIB_ID} MATCHES "rpath")
  endif (${DYLIB_ID} STREQUAL "@rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib")

elseif (UNIX AND NOT WIN32)

  set (FIRMWARE_INSTALL_DIR "/lib/firmware/qhy" CACHE STRING "QHY firmware installation directory")

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd_armv6.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd_armv8.bin")
  elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "libqhyccd_x86_64.bin")
  else ()
    message (FATAL_ERROR "x86-32 architecture is not supported.")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 85-qhyccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES qhyccd.h qhyccderr.h qhyccdcamdef.h qhyccdstruct.h DESTINATION include/libqhy)

# Install firmware
install (
   CODE "
   file(GLOB QHY_FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*) \n
   file(INSTALL DESTINATION ${FIRMWARE_INSTALL_DIR} TYPE FILE FILES \${QHY_FIRMWARE})"
)

# Install library
install_imported (TARGETS qhyccd DESTINATION ${CMAKE_INSTALL_LIBDIR})
