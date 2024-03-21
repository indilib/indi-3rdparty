cmake_minimum_required(VERSION 3.16)
project (libricohcamerasdk)

set (LIBPENTAX_VERSION "1.1.0")
set (LIBPENTAX_SOVERSION "1")
set (LIBMTP_VERSION "9.3.0")
set (LIBMTP_SOVERSION "9")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (RicohCameraSDKCpp SHARED IMPORTED)
add_library (mtpricoh          SHARED IMPORTED)

set_target_properties (RicohCameraSDKCpp PROPERTIES VERSION ${LIBPENTAX_VERSION} SOVERSION ${LIBPENTAX_SOVERSION})
set_target_properties (mtpricoh          PROPERTIES VERSION ${LIBMTP_VERSION} SOVERSION ${LIBMTP_SOVERSION})

if (UNIX AND NOT WIN32 AND NOT APPLE)

  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^(x86_64|amd64)")
    set_property (TARGET RicohCameraSDKCpp PROPERTY IMPORTED_LOCATION "lib/x64/libRicohCameraSDKCpp.bin")
    set_property (TARGET mtpricoh          PROPERTY IMPORTED_LOCATION "lib/x64/libmtpricoh.bin")
  elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^i[3-6]86")
    set_property (TARGET RicohCameraSDKCpp PROPERTY IMPORTED_LOCATION "lib/x86/libRicohCameraSDKCpp.bin")
    set_property (TARGET mtpricoh          PROPERTY IMPORTED_LOCATION "lib/x86/libmtpricoh.bin")
  elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm")
    set_property (TARGET RicohCameraSDKCpp PROPERTY IMPORTED_LOCATION "lib/armv7l/libRicohCameraSDKCpp.bin")
    set_property (TARGET mtpricoh          PROPERTY IMPORTED_LOCATION "lib/armv7l/libmtpricoh.bin")
  else ()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-pentax.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES include/ricoh_camera_sdk.hpp DESTINATION include/libricohcamerasdk)
install (DIRECTORY include/ricoh_camera_sdk DESTINATION include/libricohcamerasdk)

# Install library
install_imported (TARGETS RicohCameraSDKCpp mtpricoh DESTINATION ${CMAKE_INSTALL_LIBDIR})
