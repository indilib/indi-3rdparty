cmake_minimum_required(VERSION 3.16)
project(indi_bressercam CXX C)

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

find_package(BRESSERCAM REQUIRED)

include_directories(${BRESSERCAM_INCLUDE_DIR})

########### indi_bresser_* ###########
add_executable(indi_bresser_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_bresser_ccd PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(
  indi_bresser_ccd
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${BRESSERCAM_LIBRARIES}
  ${ZLIB_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)
add_executable(indi_bresser_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_bresser_wheel PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(
  indi_bresser_wheel
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${BRESSERCAM_LIBRARIES}
  ${ZLIB_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)
add_executable(indi_bresser_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_bresser_focuser PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(
  indi_bresser_focuser
  ${INDI_LIBRARIES}
  ${CFITSIO_LIBRARIES}
  ${BRESSERCAM_LIBRARIES}
  ${ZLIB_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

#####################################

if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
  target_link_libraries(indi_bresser_ccd rt)
  target_link_libraries(indi_bresser_wheel rt)
  target_link_libraries(indi_bresser_focuser rt)
endif(
  CMAKE_SYSTEM_NAME MATCHES "Linux"
  AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*"
)

install(
  TARGETS indi_bresser_ccd indi_bresser_wheel indi_bresser_focuser
  RUNTIME DESTINATION bin
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml
  DESTINATION ${INDI_DATA_DIR}
)
