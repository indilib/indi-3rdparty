cmake_minimum_required(VERSION 3.0)
PROJECT(libsv305 C CXX)

set(SVBCAMERASDK_VERSION "1.6.1")
set(SVBCAMERASDK_SOVERSION "1")

include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

# install binary libraries
IF("${CMAKE_SYSTEM}" MATCHES "Linux")
    message("Linux target")

    IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
        message("Intel architecture")
        IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
            # 64 bits
            message("64 bits target")
            exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_amd64.so ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})
        ELSE(CMAKE_SIZEOF_VOID_P MATCHES "8")
            # 32 bits
            message("32 bits target")
            exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_x86.so ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})
        ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")

    ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "armv6l")
        # armv6 archirecture
        message("armv6 target")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_armv6.so ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})

    ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
        # armv7 archirecture
        message("armv7 target")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_armv7.so ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})

    ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "armv8l" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        # armv8 archirecture
        message("armv8 target")
        exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libSVBCameraSDK_armv8.so ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION})

    ELSE()
        message(FATAL_ERROR "Your architecture isn't suppored")
    ENDIF()

    # install library
    install( FILES ${CMAKE_BINARY_DIR}/libSVBCameraSDK.so.${SVBCAMERASDK_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
    # create alernative symlinks
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libSVBCameraSDK.so.${SVBCAMERASDK_VERSION}\" \"libSVBCameraSDK.so.${SVBCAMERASDK_SOVERSION}\" WORKING_DIRECTORY \"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libSVBCameraSDK.so.${SVBCAMERASDK_SOVERSION}\" \"libSVBCameraSDK.so\" WORKING_DIRECTORY \"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

ELSE("${CMAKE_SYSTEM}" MATCHES "Linux")
    message(FATAL_ERROR "Library only available on Linux")
ENDIF("${CMAKE_SYSTEM}" MATCHES "Linux")

# install header files
install(FILES SVBCameraSDK.h DESTINATION include/libsv305)

# install udev rules
install(FILES 90-svbonyusb.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

# update library path
#install(CODE "execute_process(COMMAND \"ldconfig\")")


# temporary, for testing
SET(CPACK_PACKAGE_NAME "libsv305")
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "6")
# real package is 1.6.1.0-day
# I use may own numbering
SET(CPACK_PACKAGE_VERSION_PATCH "2")
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Blaise-Florentin Collin") #required
INCLUDE(CPack)
