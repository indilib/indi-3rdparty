cmake_minimum_required(VERSION 3.0)
PROJECT(libsv305 C CXX)

set(SVBCAMERASDK_VERSION "1.6.4")
set(SVBCAMERASDK_SOVERSION "1")

include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

IF (APPLE)
message(FATAL_ERROR "MacOS is not supported")
ENDIF(APPLE)

IF(UNIX AND NOT WIN32)
    IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_armv6.bin ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})
    ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_armv8.bin ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})
    ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_amd64.bin ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})
    ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_x86.bin ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})
    ENDIF()

    # install library
    install( FILES ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
    # create alernative symlinks
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libSVBCameraSDK.so.${SVBCAMERASDK_VERSION}\" \"libSVBCameraSDK.so.${SVBCAMERASDK_SOVERSION}\" WORKING_DIRECTORY \"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libSVBCameraSDK.so.${SVBCAMERASDK_SOVERSION}\" \"libSVBCameraSDK.so\" WORKING_DIRECTORY \"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

    # install udev rules
    install(FILES 90-svbonyusb.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

    # install header files
    install(FILES SVBCameraSDK.h DESTINATION include/libsv305)
ENDIF()
