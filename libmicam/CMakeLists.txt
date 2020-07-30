cmake_minimum_required(VERSION 2.8)
PROJECT(libmicam CXX C)

set(MICAM_VERSION "0.5.6")
set(MICAM_VERSION_MAC "0.4.6")
set(MICAM_SOVERSION "0")

include(GNUInstallDirs)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

IF (APPLE)

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libgxccdmac.bin ${CMAKE_BINARY_DIR}/libgxccd.${MICAM_VERSION_MAC}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libgxccd.${MICAM_VERSION_MAC}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libgxccd.${MICAM_VERSION_MAC}.dylib ${LIB_INSTALL_DIR}/libgxccd.${MICAM_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libgxccd.${MICAM_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libgxccd.dylib)

ELSE (APPLE)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libgxccdarmv7.bin ${CMAKE_BINARY_DIR}/libgxccd.so.${MICAM_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libgxccdarmv8.bin ${CMAKE_BINARY_DIR}/libgxccd.so.${MICAM_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libgxccd64.bin ${CMAKE_BINARY_DIR}/libgxccd.so.${MICAM_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libgxccd32.bin ${CMAKE_BINARY_DIR}/libgxccd.so.${MICAM_VERSION})
  ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install( FILES ${CMAKE_BINARY_DIR}/libgxccd.so.${MICAM_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libgxccd.so.${MICAM_VERSION}\" \"libgxccd.so.${MICAM_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libgxccd.so.${MICAM_SOVERSION}\" \"libgxccd.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-miccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()

ENDIF(APPLE)

install( FILES gxccd.h DESTINATION include/libmicam)
