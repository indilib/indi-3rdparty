cmake_minimum_required(VERSION 3.16)
project (libmeadecam)

set (LIBMEADECAM_VERSION "1.0")
set (LIBMEADECAM_SOVERSION "1")

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
find_package(USB1 REQUIRED)

include (GNUInstallDirs)
include (InstallImported)
include(CMakeCommon)


set(meadecam_LIB_SRCS meadecam.c)
add_library(meadecam SHARED ${meadecam_LIB_SRCS})
set_target_properties(meadecam PROPERTIES VERSION ${LIBMEADECAM_VERSION} SOVERSION ${LIBMEADECAM_SOVERSION})
target_link_libraries(meadecam ${USB1_LIBRARIES})
install(TARGETS meadecam LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (UNIX AND NOT APPLE)

  # Install udev rules
  set (UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
  install (FILES 99-meadecam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

endif ()

# Install header files
install (FILES meadecam.h DESTINATION include/libmeadecam)
