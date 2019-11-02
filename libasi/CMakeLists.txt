cmake_minimum_required(VERSION 2.8)
PROJECT(libasi CXX C)

# Using ASI Camera SDK Version 1.14.0715 updated on 2019-07-22
# Using ASI EFW SDK Version 0.3.1205 updated on 2017-12-05
# Using ASI ST4 SDK Version 1.0 updated on 2018-07-23
# Using ASI EAF SDK Version 0.1.0524 updated on 2019-06-20

set(ASICAM_VERSION "1.14.0715")
set(ASICAM_SOVERSION "1")

set(ASIEFW_VERSION "0.3.1205")
set(ASIEFW_SOVERSION "0")

set(ASIST4_VERSION "1.0")
set(ASIST4_SOVERSION "1")

set(ASIEAF_VERSION "0.1.0524")
set(ASIEAF_SOVERSION "0")

include(GNUInstallDirs)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

IF (APPLE)

# ASI CAM
exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libASICamera2.bin ${CMAKE_BINARY_DIR}/libasicamera${ASICAM_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libasicamera${ASICAM_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libasicamera${ASICAM_VERSION}.dylib ${LIB_INSTALL_DIR}/libasicamera${ASICAM_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libasicamera${ASICAM_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libasicamera.dylib)

# ASI EFW
exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libefwfilter${ASIEFW_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libefwfilter${ASIEFW_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libefwfilter${ASIEFW_VERSION}.dylib ${LIB_INSTALL_DIR}/libefwfilter${ASIEFW_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libefwfilter${ASIEFW_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libefwfilter.dylib)

# ASI ST4
exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libusb2st4conv${ASIST4_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libusb2st4conv${ASIST4_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libusb2st4conv${ASIST4_VERSION}.dylib ${LIB_INSTALL_DIR}/libusb2st4conv${ASIST4_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libusb2st4conv${ASIST4_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libusb2st4conv.dylib)

# ASI EAF
exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libeaffocuser${ASIEAF_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libeaffocuser${ASIEAF_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libeaffocuser${ASIEAF_VERSION}.dylib ${LIB_INSTALL_DIR}/libeaffocuser${ASIEAF_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libeaffocuser${ASIEAF_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libeaffocuser.dylib)

ELSE(APPLE)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv5")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv5/libASICamera2.bin ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv5/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv5/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv5/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION})
    add_definitions(-DLOW_USB_BANDWIDTH)
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libASICamera2.bin ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION})
    add_definitions(-DLOW_USB_BANDWIDTH)
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv7/libASICamera2.bin ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv7/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv7/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv7/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION})
    add_definitions(-DLOW_USB_BANDWIDTH)
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv8")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libASICamera2.bin ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION})
    add_definitions(-DLOW_USB_BANDWIDTH)
  ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libASICamera2.bin ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION})
  ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libASICamera2.bin ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libEFWFilter.bin ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libUSB2ST4Conv.bin ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION})
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libEAFFocuser.bin ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION})
  ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install( FILES ${CMAKE_BINARY_DIR}/libASICamera2.so.${ASICAM_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES ${CMAKE_BINARY_DIR}/libEFWFilter.so.${ASIEFW_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES ${CMAKE_BINARY_DIR}/libUSB2ST4Conv.so.${ASIST4_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES ${CMAKE_BINARY_DIR}/libEAFFocuser.so.${ASIEAF_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

# Make sure symbolic links are installed

## ASI CAM
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libASICamera2.so.${ASICAM_VERSION}\" \"libASICamera2.so.${ASICAM_SOVERSION}\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libASICamera2.so.${ASICAM_SOVERSION}\" \"libASICamera2.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

## ASI EFW
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libEFWFilter.so.${ASIEFW_VERSION}\" \"libEFWFilter.so.${ASIEFW_SOVERSION}\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libEFWFilter.so.${ASIEFW_SOVERSION}\" \"libEFWFilter.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

## ASI ST4
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libUSB2ST4Conv.so.${ASIST4_VERSION}\" \"libUSB2ST4Conv.so.${ASIST4_SOVERSION}\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libUSB2ST4Conv.so.${ASIST4_SOVERSION}\" \"libUSB2ST4Conv.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

## ASI EAF
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libEAFFocuser.so.${ASIEAF_VERSION}\" \"libEAFFocuser.so.${ASIEAF_SOVERSION}\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libEAFFocuser.so.${ASIEAF_SOVERSION}\" \"libEAFFocuser.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-asi.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()

ENDIF(APPLE)

install( FILES ASICamera2.h DESTINATION include/libasi)
install( FILES EFW_filter.h DESTINATION include/libasi)
install( FILES USB2ST4_Conv.h DESTINATION include/libasi)
install( FILES EAF_focuser.h DESTINATION include/libasi)
