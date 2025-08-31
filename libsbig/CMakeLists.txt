cmake_minimum_required(VERSION 3.16)
project(libsbig)

set(SBIG_VERSION "4.9.9")
set(SBIG_SOVERSION "4")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)
  if(NOT UDEVRULES_INSTALL_DIR)
    set(
      UDEVRULES_INSTALL_DIR
      "/usr/lib/udev/rules.d"
      CACHE STRING
      "Base directory for udev rules"
    )
  endif()
endif()
option(INDI_INSTALL_FIRMWARE "Install Firmware" On)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)
include(InstallImported)

add_library(sbig SHARED IMPORTED)

set_target_properties(
  sbig
  PROPERTIES VERSION ${SBIG_VERSION} SOVERSION ${SBIG_SOVERSION}
)

if(APPLE)
  set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/sbig")

  set_property(TARGET sbig PROPERTY IMPORTED_LOCATION "SBIGUDrvOSX.bin")

  fix_macos_libraries("libsbig" "SBIGUDrvOSX.bin" "SBIG")
elseif(UNIX AND NOT WIN32)
  set(
    FIRMWARE_INSTALL_DIR
    "/usr/lib/firmware"
    CACHE STRING
    "sbig firmware installation directory"
  )

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property(TARGET sbig PROPERTY IMPORTED_LOCATION "libsbig_armhf.bin")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property(TARGET sbig PROPERTY IMPORTED_LOCATION "libsbig_arm64.bin")
  elseif(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set_property(TARGET sbig PROPERTY IMPORTED_LOCATION "libsbig_x64.bin")
  endif()

  # Install udev rules
  if(INDI_INSTALL_UDEV_RULES)
    install(FILES 51-sbig-debian.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
  endif()
endif()

# Install header files
install(FILES sbigudrv.h DESTINATION include/libsbig)

# Install firmware
if(INDI_INSTALL_FIRMWARE)
  install(
    FILES sbigucam.hex sbiglcam.hex sbigfcam.hex sbigpcam.hex stfga.bin
    DESTINATION ${FIRMWARE_INSTALL_DIR}
  )
endif()

# Install library
install_imported(TARGETS sbig DESTINATION ${CMAKE_INSTALL_LIBDIR})
