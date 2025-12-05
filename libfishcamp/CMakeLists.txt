cmake_minimum_required(VERSION 3.16)
project(libfishcamp C CXX)

#***********************************************************
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)
find_package(INDI REQUIRED)

option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)
option(INDI_INSTALL_FIRMWARE "Install Firmware" on)

include_directories(${INDI_INCLUDE_DIR})

if(APPLE)
  set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/fishcamp")
  ##This one is needed for homebrew
  include_directories("/usr/local/include")
  ## This one is needed for Craft
  include_directories("${CMAKE_INSTALL_PREFIX}/include")
else()
  if(NOT DEFINED FIRMWARE_INSTALL_DIR)
    set(
      FIRMWARE_INSTALL_DIR
      "/usr/lib/firmware"
      CACHE STRING
      "libfishcamp firmware installation dir"
    )
  endif()
  if(NOT DEFINED UDEVRULES_INSTALL_DIR)
    set(
      UDEVRULES_INSTALL_DIR
      "/usr/lib/udev/rules.d"
      CACHE STRING
      "Base directory for udev rules"
    )
  endif()
endif(APPLE)
#***********************************************************
find_package(USB1 REQUIRED)
include_directories(${USB1_INCLUDE_DIR})
add_definitions(-Wno-multichar)

set(LIBFISHCAMP_VERSION "1.1")
set(LIBFISHCAMP_SOVERSION "1")

set(fishcamp_LIB_SRCS fishcamp.c)

#build a shared library
add_library(fishcamp SHARED ${fishcamp_LIB_SRCS})

set_target_properties(
  fishcamp
  PROPERTIES VERSION ${LIBFISHCAMP_VERSION} SOVERSION ${LIBFISHCAMP_SOVERSION}
)

target_link_libraries(fishcamp ${USB1_LIBRARIES})

install(FILES fishcamp.h fishcamp_common.h DESTINATION include/libfishcamp)

install(TARGETS fishcamp LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(INDI_INSTALL_FIRMWARE)
  install(
    FILES gdr_usb.hex Guider_mono_rev16_intel.srec
    DESTINATION ${FIRMWARE_INSTALL_DIR}
  )
  if(NOT APPLE AND INDI_INSTALL_UDEV_RULES)
    install(FILES 99-fishcamp.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
  endif()
endif()
