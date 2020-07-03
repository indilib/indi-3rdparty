cmake_minimum_required(VERSION 2.8)
PROJECT(libqhy C CXX)

include(GNUInstallDirs)

# QHY SDK 20.06.26
set(LIBQHY_VERSION "20.3.26")
set(LIBQHY_SOVERSION "20")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

IF (APPLE)

set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/qhy/firmware" CACHE STRING "QHY firmware installation directory")

# One problem that is often encountered on macs when using rpaths is that the ID the dylib has does not match the links being made to the dylib
# Since when using RPATHS, programs link to libraries using their IDs, that would cause a problem later when a program being built tries to link to the wrong name for the dylib
# This should get the current ID of the dylib and make sure it matches what we expect it to be, and stops the build if it does not.
execute_process(COMMAND bash "-c" "otool -D ${CMAKE_CURRENT_SOURCE_DIR}/libqhyccd.dylib | tail -n 1 | tr -d '\n'" OUTPUT_VARIABLE DYLIB_ID)
if(${DYLIB_ID} STREQUAL "@rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib")
  message(STATUS "The Mac dylib in the repo matches the libqhy SDK in the repo, linking should succeed.")
else(${DYLIB_ID} STREQUAL "@rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib")
  if(${DYLIB_ID} MATCHES "rpath")
    message(FATAL_ERROR "The Mac dylib in the libqhy source folder does not match the current SDK Version.  Linking will fail since it uses rpaths for linking, since the library is called ${DYLIB_ID} and the linker thinks it should be called @rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib.  Please request that the library be updated in the repo.")
  else(${DYLIB_ID} MATCHES "rpath")
    message(WARNING "The Mac dylib does not have an RPATH ID, you might have issues linking it later since it probably uses an absoute path.  Please request that the ID be changed in the repo.")
  endif(${DYLIB_ID} MATCHES "rpath")
endif(${DYLIB_ID} STREQUAL "@rpath/libqhyccd.${LIBQHY_SOVERSION}.dylib")

# remove all existing libqhyccd
exec_program(rm ARGS ${LIB_INSTALL_DIR}/libqhyccd*)

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhyccd.dylib ${CMAKE_BINARY_DIR}/libqhyccd.${LIBQHY_VERSION}.dylib)
exec_program(cp ARGS ${CMAKE_BINARY_DIR}/libqhyccd.${LIBQHY_VERSION}.dylib ${LIB_INSTALL_DIR}/libqhyccd.${LIBQHY_VERSION}.dylib)

exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libqhyccd.${LIBQHY_VERSION}.dylib ${LIB_INSTALL_DIR}/libqhyccd.${LIBQHY_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libqhyccd.${LIBQHY_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libqhyccd.dylib)

# Make sure symbolic links are installed
# Looks like this only works on UNIX
#file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_libqhy_symlink.cmake "
#exec_program(${CMAKE_COMMAND} ARGS -E create_symlink libqhyccd.${LIBQHY_VERSION}.dylib ${LIB_INSTALL_DIR}/libqhyccd.${LIBQHY_SOVERSION}.dylib)\n
#exec_program(${CMAKE_COMMAND} ARGS -E create_symlink libqhyccd.${LIBQHY_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libqhyccd.dylib)\n
#")

ELSE()

set(FIRMWARE_INSTALL_DIR "/lib/firmware/qhy" CACHE STRING "QHY firmware installation directory")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
 IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhyccd_armv6.bin ${CMAKE_BINARY_DIR}/libqhyccd.so.${LIBQHY_VERSION})
 ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhyccd_armv8.bin ${CMAKE_BINARY_DIR}/libqhyccd.so.${LIBQHY_VERSION})
 ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhyccd_x86_64.bin ${CMAKE_BINARY_DIR}/libqhyccd.so.${LIBQHY_VERSION})
 ELSE()
  message(FATAL_ERROR "x86-32 architecture is not supported.")
 ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install(FILES 85-qhyccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
install(FILES ${CMAKE_BINARY_DIR}/libqhyccd.so.${LIBQHY_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libqhyccd.so.${LIBQHY_VERSION}\" \"libqhyccd.so.${LIBQHY_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libqhyccd.so.${LIBQHY_SOVERSION}\" \"libqhyccd.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

ENDIF (APPLE)

# Install header files
install(FILES qhyccd.h qhyccderr.h qhyccdcamdef.h qhyccdstruct.h DESTINATION include/libqhy)

# Install firmware
install(
   CODE "
   file(GLOB QHY_FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*) \n
   file(INSTALL DESTINATION ${FIRMWARE_INSTALL_DIR} TYPE FILE FILES \${QHY_FIRMWARE})"
 )
