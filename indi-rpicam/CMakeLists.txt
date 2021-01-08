cmake_minimum_required(VERSION 3.0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

PROJECT(indi_rpicam CXX C)

set(INDI_RPICAM_VERSION_MAJOR 1)
set(INDI_RPICAM_VERSION_MINOR 3)

if (NOT DEFINED VC_ROOT)
    set(VC_ROOT "/opt/vc")
endif(NOT DEFINED VC_ROOT)

set(CMAKE_PREFIX_PATH ${VC_ROOT})

# ISO does not seem to be supported by the camere. If used, the gain stops working. So leave it off
option(USE_ISO "Enable ISO settings Subsystem" OFF)

include(GNUInstallDirs)
include(CMakeCommon)

find_package(MMAL REQUIRED)
find_package(INDI COMPONENTS driver REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(Nova REQUIRED)
find_package(USB1 REQUIRED)
find_package(FFTW3 REQUIRED)
find_package(FFTW3 REQUIRED)

find_library(M_LIBRARY NAMES m)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${MMAL_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})

set(LIB_RPICAM_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/mmalcamera.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mmaldriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mmalencoder.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mmalexception.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mmalcomponent.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cameracontrol.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/raw10tobayer16pipeline.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/raw12tobayer16pipeline.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pipeline.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpegpipeline.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/broadcompipeline.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pipetee.cpp
)

add_library(rpicam STATIC ${LIB_RPICAM_SRCS})

add_executable(indi_rpicam ${CMAKE_CURRENT_SOURCE_DIR}/indi_rpicam.cpp)

target_link_libraries(indi_rpicam
    rpicam
    ${INDI_DRIVER_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${INDI_LIBRARIES}
    ${MMAL_LIBRARIES}
    ${Threads_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_rpicam.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_rpicam.xml )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

install(TARGETS indi_rpicam RUNTIME DESTINATION bin)

find_package (GTest)
find_package (GMock)

set(INDI_BUILD_UNITTESTS TRUE)

IF (GTEST_FOUND)
  IF (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Building unit tests")
    ADD_SUBDIRECTORY(test)
  ELSE (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Not building unit tests")
  ENDIF (INDI_BUILD_UNITTESTS)
ELSE()
  MESSAGE (STATUS  "GTEST not found, not building unit tests")
ENDIF (GTEST_FOUND)

# ARM specific flags
include(FindARM.cmake)
IF (NEON_FOUND)
  MESSAGE(STATUS "Neon found with compiler flag : -mfpu=neon -D__NEON__")
  SET(CMAKE_C_FLAGS "-mfpu=neon -D__NEON__ -ftree-vectorize ${CMAKE_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "-mfpu=neon -D__NEON__ -ftree-vectorize ${CMAKE_CXX_FLAGS}")
ENDIF (NEON_FOUND)
IF (CORTEXA8_FOUND)
  MESSAGE(STATUS "Cortex-A8 Found with compiler flag : -mcpu=cortex-a8")
  SET(CMAKE_C_FLAGS "-mcpu=cortex-a8 -fprefetch-loop-arrays ${CMAKE_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "-mcpu=cortex-a8 -fprefetch-loop-arrays ${CMAKE_CXX_FLAGS}")
ENDIF (CORTEXA8_FOUND)
IF (CORTEXA9_FOUND)
  MESSAGE(STATUS "Cortex-A9 Found with compiler flag : -mcpu=cortex-a9")
  SET(CMAKE_C_FLAGS "-mcpu=cortex-a9 ${CMAKE_C_FLAGS}")
  SET(CMAKE_CXX_FLAGS "-mcpu=cortex-a9 ${CMAKE_CXX_FLAGS}")
ENDIF (CORTEXA9_FOUND)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_rpicam.xml CONFIGURATIONS Release DESTINATION ${INDI_DATA_DIR})
