cmake_minimum_required(VERSION 3.0)
PROJECT(libricohcamerasdk CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

set(LIBPENTAX_VERSION "1.1.0")
set(LIBPENTAX_SOVERSION "1")
set(LIBMTP_VERSION "9.3.0")
set(LIBMTP_SOVERSION "9")

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(x86_64|amd64)")
  set(architecture_path "x64")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^i[3-6]86")
  set(architecture_path "x86")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm")
  set(architecture_path "armv7l")
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/lib/${architecture_path}/libRicohCameraSDKCpp.bin ${CMAKE_BINARY_DIR}/libRicohCameraSDKCpp.so.${LIBPENTAX_VERSION})
exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/lib/${architecture_path}/libmtpricoh.bin ${CMAKE_BINARY_DIR}/libmtpricoh.so.${LIBMTP_VERSION})

install( FILES ${CMAKE_BINARY_DIR}/libRicohCameraSDKCpp.so.${LIBPENTAX_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES ${CMAKE_BINARY_DIR}/libmtpricoh.so.${LIBMTP_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libRicohCameraSDKCpp.so.${LIBPENTAX_VERSION}\" \"libRicohCameraSDKCpp.so.${LIBPENTAX_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libRicohCameraSDKCpp.so.${LIBPENTAX_SOVERSION}\" \"libRicohCameraSDKCpp.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libmtpricoh.so.${LIBMTP_VERSION}\" \"libmtpricoh.so.${LIBMTP_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libmtpricoh.so.${LIBMTP_SOVERSION}\" \"libmtpricoh.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-pentax.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()

install( FILES include/ricoh_camera_sdk.hpp DESTINATION include/libricohcamerasdk)
install( DIRECTORY include/ricoh_camera_sdk DESTINATION include/libricohcamerasdk)

