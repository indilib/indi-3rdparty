cmake_minimum_required(VERSION 3.16)
project(indi_toupbase CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)

set(
  UDEVRULES_INSTALL_DIR
  "/lib/udev/rules.d"
  CACHE STRING
  "Base directory for udev rules"
)

set(TOUPBASE_VERSION_MAJOR 2)
set(TOUPBASE_VERSION_MINOR 4)

if(PROJECT_IS_TOP_LEVEL)
  option(WITH_TOUPCAM "Install Toupcam Driver" On)
  option(WITH_ALTAIRCAM "Install Altaircam Driver" On)
  option(WITH_BRESSERCAM "Install Bressercam Driver" On)
  option(WITH_MALLINCAM "Install Mallingcam Driver" On)
  option(WITH_MEADECAM "Install Meadecam Driver" On)
  option(WITH_NNCAM "Install NNcam Driver" On)
  option(WITH_OGMACAM "Install Ogmacam Driver" On)
  option(WITH_OMEGONPROCAM "Install Omegonprocam Driver" On)
  option(WITH_STARSHOOTG "Install Starshootg Driver" On)
  option(WITH_TSCAM "Install TScam Driver" On)
  option(WITH_SVBONYCAM "Install Svbonycam Driver" On)
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

set(
  indi_toupbase_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp
)
set(
  indi_wheel_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupwheel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp
)
set(
  indi_focuser_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_focuser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp
)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})

macro(build_touptek_driver BRAND LABEL MANUFACTURER DRIVER_NAME)
  string(TOLOWER ${BRAND} BRAND_LOWER)
  set(LABEL ${LABEL})
  set(MANUFACTURER ${MANUFACTURER})
  set(DRIVER_NAME ${DRIVER_NAME})
  string(TOLOWER ${BRAND} DRIVER_LIB_NAME)

  find_package(${BRAND} REQUIRED)
  include_directories(${${BRAND}_INCLUDE_DIR})

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_${BRAND_LOWER}.xml
  )

  add_executable(indi_${BRAND_LOWER}_ccd ${indi_toupbase_SRCS})
  target_compile_definitions(indi_${BRAND_LOWER}_ccd PRIVATE "-DBUILD_${BRAND}")
  target_link_libraries(
    indi_${BRAND_LOWER}_ccd
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${${BRAND}_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  add_executable(indi_${BRAND_LOWER}_wheel ${indi_wheel_SRCS})
  target_compile_definitions(indi_${BRAND_LOWER}_wheel PRIVATE "-DBUILD_${BRAND}")
  target_link_libraries(
    indi_${BRAND_LOWER}_wheel
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${${BRAND}_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  add_executable(indi_${BRAND_LOWER}_focuser ${indi_focuser_SRCS})
  target_compile_definitions(indi_${BRAND_LOWER}_focuser PRIVATE "-DBUILD_${BRAND}")
  target_link_libraries(
    indi_${BRAND_LOWER}_focuser
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${${BRAND}_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  if(
    CMAKE_SYSTEM_NAME MATCHES "Linux"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*"
  )
    target_link_libraries(indi_${BRAND_LOWER}_ccd rt)
    target_link_libraries(indi_${BRAND_LOWER}_wheel rt)
    target_link_libraries(indi_${BRAND_LOWER}_focuser rt)
  endif()

  install(
    TARGETS indi_${BRAND_LOWER}_ccd indi_${BRAND_LOWER}_wheel indi_${BRAND_LOWER}_focuser
    RUNTIME DESTINATION bin
  )
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_${BRAND_LOWER}.xml
    DESTINATION ${INDI_DATA_DIR}
  )
endmacro()

########### indi_toupcam_* ###########
if(WITH_TOUPCAM)
  build_touptek_driver("TOUPCAM" "Toupcam" "Touptek" "Touptek")

  add_executable(toupcam_test toupcam_test.cpp)
  target_link_libraries(
    toupcam_test
    ${TOUPCAM_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
  )
endif()

########### indi_altair_* ###########
if(WITH_ALTAIRCAM)
  build_touptek_driver("ALTAIRCAM" "Altair" "Altair" "Altair")
endif()

########### indi_bressercam_* ###########
if(WITH_BRESSERCAM)
  build_touptek_driver("BRESSERCAM" "Bresser" "Bresser" "Bresser")
endif()

########### indi_ogmacam_* ###########
if(WITH_OGMACAM)
  build_touptek_driver("OGMACAM" "Ogmacam" "OGMAVision" "OGMAVision")
endif()

########### indi_tscam_* ###########
if(WITH_TSCAM)
  build_touptek_driver("TSCAM" "Tscam" "Teleskop" "Teleskop")
endif()

########### indi_svbonycam_* ###########
if(WITH_SVBONYCAM)
  build_touptek_driver("SVBONYCAM" "SVbonycam" "SVBONY2" "SVBONY2")
endif()

########### indi_starshootg_* ###########
if(WITH_STARSHOOTG)
  build_touptek_driver("STARSHOOTG" "StarShootG" "Orion" "Orion")
endif()

########### indi_nncam_* ###########
if(WITH_NNCAM)
  build_touptek_driver("NNCAM" "Nncam" "Nn" "Nn")
endif()

########### indi_mallincam_* ###########
if(WITH_MALLINCAM)
  build_touptek_driver("MALLINCAM" "Mallincam" "Mallincam" "MALLINCAM")
endif()

########### indi_omegonprocam_* ###########
if(WITH_OMEGONPROCAM)
  build_touptek_driver("OMEGONPROCAM" "OmegonPro" "Omegon" "Astroshop")

  add_executable(omegonprocam_test omegonprocam_test.cpp)
  target_link_libraries(
    omegonprocam_test
    ${OMEGONPROCAM_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
  )
endif()

########### indi_meadecam_* ###########
if(WITH_MEADECAM)
  build_touptek_driver("MEADECAM" "Meadecam" "Meade" "Meade")
endif()
