cmake_minimum_required(VERSION 3.16)
project (libqhy)

# QHY SDK 26.07.21
set (LIBQHY_VERSION "26.7.21")
set (LIBQHY_SOVERSION "20")

option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)
option(INDI_INSTALL_FIRMWARE "Install Firmware" On)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (qhyccd SHARED IMPORTED)

set_target_properties (qhyccd PROPERTIES VERSION ${LIBQHY_VERSION} SOVERSION ${LIBQHY_SOVERSION})

if (APPLE)

  set (QHY_FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/qhy/firmware" CACHE STRING "QHY firmware installation directory")

  set(source_mac_dir "mac_x64")
  if (CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(source_mac_dir "mac_arm64")
  endif()

  set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "${source_mac_dir}/libqhyccd.bin")

  FIX_MACOS_LIBRARIES("libqhyccd" "${source_mac_dir}/libqhyccd.bin" "QHY")

elseif (UNIX AND NOT WIN32)

  if (NOT DEFINED QHY_FIRMWARE_INSTALL_DIR)
    set (QHY_FIRMWARE_INSTALL_DIR "/usr/lib/firmware/qhy" CACHE STRING "QHY firmware installation directory")
  endif()

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "armv6/libqhyccd.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "armv8/libqhyccd.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set_property (TARGET qhyccd PROPERTY IMPORTED_LOCATION "x64/libqhyccd.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    message (FATAL_ERROR "x86-32 architecture is not supported.")
  endif ()

  # Install udev rules
  if (INDI_INSTALL_UDEV_RULES)
    if (NOT DEFINED UDEVRULES_INSTALL_DIR)
      set (UDEVRULES_INSTALL_DIR "/usr/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
    endif()
 
    SET (UDEVRULES_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/85-qhyccd.rules")
    SET (UDEVRULES_GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/85-qhyccd.rules")

    add_custom_command(
      OUTPUT "${UDEVRULES_GENERATED_FILE}"
      COMMAND sed -e "s@/lib/firmware/qhy@${QHY_FIRMWARE_INSTALL_DIR}@g" "${UDEVRULES_SOURCE_FILE}" > "${UDEVRULES_GENERATED_FILE}"
      DEPENDS "${UDEVRULES_SOURCE_FILE}"
    )
    add_custom_target(
      QHY_CCD_UDEVRULE_GENERATED_TARGET
      ALL
      DEPENDS "${UDEVRULES_GENERATED_FILE}"
    )

    install (FILES ${UDEVRULES_GENERATED_FILE} DESTINATION ${UDEVRULES_INSTALL_DIR})
  endif ()
endif ()

# Install header files
install (FILES qhyccd.h qhyccderr.h qhyccdcamdef.h qhyccdstruct.h DESTINATION include/libqhy)

# Install firmware
IF (INDI_INSTALL_FIRMWARE)
  install (
    CODE "
    file(GLOB QHY_FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*) \n
    file(INSTALL DESTINATION ${QHY_FIRMWARE_INSTALL_DIR} TYPE FILE FILES \${QHY_FIRMWARE})"
  )
endif()

# Install library
install_imported (TARGETS qhyccd DESTINATION ${CMAKE_INSTALL_LIBDIR})
