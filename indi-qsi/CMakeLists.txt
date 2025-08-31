cmake_minimum_required(VERSION 3.16)
project(indi_qsi CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

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

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(QSI REQUIRED)
find_package(ZLIB REQUIRED)

set(QSI_VERSION_MAJOR 0)
set(QSI_VERSION_MINOR 9)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_qsi.xml.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/indi_qsi.xml
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})
include_directories(${QSI_INCLUDE_DIR})

include(CMakeCommon)

########### QSI ###########
set(indiqsi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/qsi_ccd.cpp)

add_executable(indi_qsi_ccd ${indiqsi_SRCS})

target_link_libraries(
  indi_qsi_ccd
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${QSI_LIBRARIES}
)

install(TARGETS indi_qsi_ccd RUNTIME DESTINATION bin)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_qsi.xml
  DESTINATION ${INDI_DATA_DIR}
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND INDI_INSTALL_UDEV_RULES)
  install(FILES 99-qsi.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
