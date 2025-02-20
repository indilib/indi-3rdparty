cmake_minimum_required(VERSION 3.16)
project (libasi)

# Using ASI Camera SDK Version 1.37 updated on 2024-12-03
# Using ASI EFW SDK Version 1.7 updated on 2021-05-17
# Using ASI ST4 SDK Version 1.0 updated on 2018-07-23
# Using ASI EAF SDK Version 1.6 updated on 2023-03-16
# Using ASI CAA SDK Version 1.0 updated on 2025-01-09

set (ASICAM_VERSION "1.37")
set (ASICAM_SOVERSION "1")

set (ASIEFW_VERSION "1.7")
set (ASIEFW_SOVERSION "1")

set (ASIST4_VERSION "1.0")
set (ASIST4_SOVERSION "1")

set (ASIEAF_VERSION "1.6")
set (ASIEAF_SOVERSION "1")

set (ASICAA_VERSION "1.0")
set (ASICAA_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (ASICamera2  SHARED IMPORTED)
add_library (EFWFilter   SHARED IMPORTED)
add_library (USB2ST4Conv SHARED IMPORTED)
add_library (EAFFocuser  SHARED IMPORTED)
add_library (CAARotator  SHARED IMPORTED)

set_target_properties (ASICamera2  PROPERTIES VERSION ${ASICAM_VERSION} SOVERSION ${ASICAM_SOVERSION})
set_target_properties (EFWFilter   PROPERTIES VERSION ${ASIEFW_VERSION} SOVERSION ${ASIEFW_SOVERSION})
set_target_properties (USB2ST4Conv PROPERTIES VERSION ${ASIST4_VERSION} SOVERSION ${ASIST4_SOVERSION})
set_target_properties (EAFFocuser  PROPERTIES VERSION ${ASIEAF_VERSION} SOVERSION ${ASIEAF_SOVERSION})
set_target_properties (CAARotator  PROPERTIES VERSION ${ASICAA_VERSION} SOVERSION ${ASICAA_SOVERSION})

if (APPLE)

  set_property (TARGET ASICamera2  PROPERTY IMPORTED_LOCATION "mac/libASICamera2.bin")
  set_property (TARGET EFWFilter   PROPERTY IMPORTED_LOCATION "mac/libEFWFilter.bin")
  set_property (TARGET USB2ST4Conv PROPERTY IMPORTED_LOCATION "mac/libUSB2ST4Conv.bin")
  set_property (TARGET EAFFocuser  PROPERTY IMPORTED_LOCATION "mac/libEAFFocuser.bin")
  set_property (TARGET CAARotator  PROPERTY IMPORTED_LOCATION "mac/libCAARotator.bin")

  FIX_MACOS_LIBRARIES("libASICamera2" "mac/libASICamera2.bin" "ASI/ZWO")
  FIX_MACOS_LIBRARIES("libEFWFilter" "mac/libEFWFilter.bin" "ASI/ZWO")
  FIX_MACOS_LIBRARIES("libUSB2ST4Conv" "mac/libUSB2ST4Conv.bin" "ASI/ZWO")
  FIX_MACOS_LIBRARIES("libEAFFocuser" "mac/libEAFFocuser.bin" "ASI/ZWO")
  FIX_MACOS_LIBRARIES("libCAARotator" "mac/libCAARotator.bin" "ASI/ZWO")

elseif (UNIX AND NOT WIN32)

  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set_property (TARGET ASICamera2  PROPERTY IMPORTED_LOCATION "armv6/libASICamera2.bin")
    set_property (TARGET EFWFilter   PROPERTY IMPORTED_LOCATION "armv6/libEFWFilter.bin")
    set_property (TARGET USB2ST4Conv PROPERTY IMPORTED_LOCATION "armv6/libUSB2ST4Conv.bin")
    set_property (TARGET EAFFocuser  PROPERTY IMPORTED_LOCATION "armv6/libEAFFocuser.bin")
    set_property (TARGET CAARotator  PROPERTY IMPORTED_LOCATION "armv6/libCAARotator.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET ASICamera2  PROPERTY IMPORTED_LOCATION "armv8/libASICamera2.bin")
    set_property (TARGET EFWFilter   PROPERTY IMPORTED_LOCATION "armv8/libEFWFilter.bin")
    set_property (TARGET USB2ST4Conv PROPERTY IMPORTED_LOCATION "armv8/libUSB2ST4Conv.bin")
    set_property (TARGET EAFFocuser  PROPERTY IMPORTED_LOCATION "armv8/libEAFFocuser.bin")
    set_property (TARGET CAARotator  PROPERTY IMPORTED_LOCATION "armv8/libCAARotator.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set_property (TARGET ASICamera2  PROPERTY IMPORTED_LOCATION "x64/libASICamera2.bin")
    set_property (TARGET EFWFilter   PROPERTY IMPORTED_LOCATION "x64/libEFWFilter.bin")
    set_property (TARGET USB2ST4Conv PROPERTY IMPORTED_LOCATION "x64/libUSB2ST4Conv.bin")
    set_property (TARGET EAFFocuser  PROPERTY IMPORTED_LOCATION "x64/libEAFFocuser.bin")
    set_property (TARGET CAARotator  PROPERTY IMPORTED_LOCATION "x64/libCAARotator.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set_property (TARGET ASICamera2  PROPERTY IMPORTED_LOCATION "x86/libASICamera2.bin")
    set_property (TARGET EFWFilter   PROPERTY IMPORTED_LOCATION "x86/libEFWFilter.bin")
    set_property (TARGET USB2ST4Conv PROPERTY IMPORTED_LOCATION "x86/libUSB2ST4Conv.bin")
    set_property (TARGET EAFFocuser  PROPERTY IMPORTED_LOCATION "x86/libEAFFocuser.bin")
    set_property (TARGET CAARotator  PROPERTY IMPORTED_LOCATION "x86/libCAARotator.bin")
  endif ()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-asi.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (
  FILES ASICamera2.h EFW_filter.h USB2ST4_Conv.h EAF_focuser.h CAA_API.h
  DESTINATION include/libasi
)

# Install library
install_imported (
  TARGETS ASICamera2 EFWFilter USB2ST4Conv EAFFocuser CAARotator
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
