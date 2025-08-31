cmake_minimum_required(VERSION 3.16)
project(indi_nightscape CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)


find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
find_package(D2XX)
find_package(USB1 REQUIRED)
find_package(FTDI1 REQUIRED)

if(D2XX_FOUND)
  set(HAVE_D2XX 1)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_D2XX")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_D2XX")
endif()

if(APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DARWIN_C_SOURCE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DARWIN_C_SOURCE")
endif()

set(INDI_NIGHTSCAPE_VERSION_MAJOR 1)
set(INDI_NIGHTSCAPE_VERSION_MINOR 0)

#set (HAVE_SERIAL 1)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_nightscape.xml.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/indi_nightscape.xml
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})
include_directories(${USB1_INCLUDE_DIRS})
include_directories(${FTDI1_INCLUDE_DIRS})

include(CMakeCommon)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-error")

set(
  indinightscape_CORE
  ${CMAKE_CURRENT_SOURCE_DIR}/nschannel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/nschannel-u.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/nsmsg.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/nsdownload.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/nsstatus.cpp
)

if(HAVE_D2XX)
  set(
    indinightscape_CORE
    ${indinightscape_CORE}
    ${CMAKE_CURRENT_SOURCE_DIR}/nschannel-ftd.cpp
  )
endif()

if(HAVE_SERIAL)
  set(
    indinightscape_CORE
    ${indinightscape_CORE}
    ${CMAKE_CURRENT_SOURCE_DIR}/nschannel-ser.cpp
  )

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SERIAL")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SERIAL")
endif()

set(
  indinightscape_SRCS
  ${indinightscape_CORE}
  ${CMAKE_CURRENT_SOURCE_DIR}/nightscape.cpp
)

set(
  nstest_SRCS
  ${indinightscape_CORE}
  ${CMAKE_CURRENT_SOURCE_DIR}/nstest-main.cpp
)

add_executable(indi_nightscape_ccd ${indinightscape_SRCS})

add_executable(nstest ${nstest_SRCS})

if(HAVE_D2XX)
  target_link_libraries(
    indi_nightscape_ccd
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${D2XX_LIBRARIES}
    ${FTDI1_LIBRARIES}
    ${USB1_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  target_link_libraries(
    nstest
    ${D2XX_LIBRARIES}
    ${FTDI1_LIBRARIES}
    ${USB1_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )
else()
  target_link_libraries(
    indi_nightscape_ccd
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${FTDI1_LIBRARIES}
    ${USB1_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  target_link_libraries(
    nstest
    ${FTDI1_LIBRARIES}
    ${USB1_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )
endif()

install(TARGETS indi_nightscape_ccd RUNTIME DESTINATION bin)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_nightscape.xml
  DESTINATION ${INDI_DATA_DIR}
)

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

  if(INDI_INSTALL_UDEV_RULES)
    install(
      FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-nightscape.rules
      DESTINATION ${UDEVRULES_INSTALL_DIR}
    )
  endif()
endif()
