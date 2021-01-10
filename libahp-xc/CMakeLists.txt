cmake_minimum_required(VERSION 3.0)
PROJECT(libahp_xc C CXX)

set(AHPXC_VERSION "1.0.10")
set(AHPXC_SOVERSION "1")

include(GNUInstallDirs)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

IF(UNIX AND NOT WIN32 AND NOT APPLE)
 IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libahp_xc.so.armhf ${CMAKE_BINARY_DIR}/libahp_xc.so.${AHPXC_VERSION})
 ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libahp_xc.so.arm64 ${CMAKE_BINARY_DIR}/libahp_xc.so.${AHPXC_VERSION})
 ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libahp_xc.so.amd64 ${CMAKE_BINARY_DIR}/libahp_xc.so.${AHPXC_VERSION})
 ELSE(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libahp_xc.so.i386 ${CMAKE_BINARY_DIR}/libahp_xc.so.${AHPXC_VERSION})
 ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)

install( FILES ${CMAKE_BINARY_DIR}/libahp_xc.so.${AHPXC_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES ahp_xc.h DESTINATION include/ahp)
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libahp_xc.so.${AHPXC_VERSION}\" \"libahp_xc.so.${AHPXC_SOVERSION}\" 
WORKING_DIRECTORY \"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libahp_xc.so.${AHPXC_SOVERSION}\" \"libahp_xc.so\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
