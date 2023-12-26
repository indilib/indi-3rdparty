cmake_minimum_required(VERSION 3.16)
project (libinovasdk)

set (INOVASDK_VERSION "1.3.6")
set (INOVASDK_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include (GNUInstallDirs)
include (InstallImported)

add_library (inovasdk SHARED IMPORTED)

set_target_properties (inovasdk PROPERTIES VERSION 1.3.6 SOVERSION 1)

if (UNIX AND NOT WIN32 AND NOT APPLE)

   if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
      set_property (TARGET inovasdk PROPERTY IMPORTED_LOCATION "libinovasdk.so.armv6")
   #elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
   #   set_property (TARGET inovasdk PROPERTY IMPORTED_LOCATION "libinovasdk.so.armv7")
   elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
      set_property (TARGET inovasdk PROPERTY IMPORTED_LOCATION "libinovasdk.so.armv8")
   elseif (CMAKE_SIZEOF_VOID_P MATCHES "8")
      set_property (TARGET inovasdk PROPERTY IMPORTED_LOCATION "libinovasdk.so.x64")
   else ()
      set_property (TARGET inovasdk PROPERTY IMPORTED_LOCATION "libinovasdk.so.x86")
   endif ()

   # Install udev rules
   set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
   install (FILES 99-inovaplx.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES inovasdk.h DESTINATION include/inovasdk)

# Install library
install_imported (TARGETS inovasdk DESTINATION ${CMAKE_INSTALL_LIBDIR})
