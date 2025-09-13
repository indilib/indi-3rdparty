cmake_minimum_required(VERSION 3.16)
project(indi_altaircam CXX C)

include(GNUInstallDirs)
include(CMakeCommon)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

set(
  UDEVRULES_INSTALL_DIR
  "/lib/udev/rules.d"
  CACHE STRING
  "Base directory for udev rules"
)

set(TOUPBASE_VERSION_MAJOR 2)
set(TOUPBASE_VERSION_MINOR 3)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.xml.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml
)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})

set(
  indi_toupbase_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/indi_toupbase.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/libtoupbase.cpp
)
set(
  indi_wheel_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/indi_toupwheel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/libtoupbase.cpp
)
set(
  indi_focuser_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/indi_focuser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../indi-toupbase/libtoupbase.cpp
)

find_package(ALTAIRCAM REQUIRED)

include_directories(${ALTAIRCAM_INCLUDE_DIR})

########### indi_altair_* ###########
add_executable(indi_altair_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_altair_ccd PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(
  indi_altair_ccd
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${ALTAIRCAM_LIBRARIES}
  ${ZLIB_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)
add_executable(indi_altair_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_altair_wheel PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(
  indi_altair_wheel
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${ALTAIRCAM_LIBRARIES}
  ${ZLIB_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)
add_executable(indi_altair_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_altair_focuser PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(
  indi_altair_focuser
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${ALTAIRCAM_LIBRARIES}
  ${ZLIB_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

#####################################

if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
  target_link_libraries(indi_altair_ccd rt)
  target_link_libraries(indi_altair_wheel rt)
  target_link_libraries(indi_altair_focuser rt)
endif(
  CMAKE_SYSTEM_NAME MATCHES "Linux"
  AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*"
)

install(
  TARGETS indi_altair_ccd indi_altair_wheel indi_altair_focuser
  RUNTIME DESTINATION bin
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml
  DESTINATION ${INDI_DATA_DIR}
)
