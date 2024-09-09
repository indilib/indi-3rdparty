cmake_minimum_required(VERSION 3.20)
PROJECT(indi_libcamera CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)
find_package(LibCamera REQUIRED)
find_package(LibCameraApps REQUIRED)
find_package(LibRaw REQUIRED)
find_package(JPEG REQUIRED)
find_package(Boost COMPONENTS program_options)
find_package(PkgConfig REQUIRED)
find_library(EXIF_LIBRARY exif REQUIRED)


set(LIBCAMERA_VERSION_MAJOR 1)
set(LIBCAMERA_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_libcamera.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_libcamera.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${LibRaw_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${LibCamera_INCLUDE_DIR})
include_directories( ${LibCameraApps_INCLUDE_DIR})

include(CMakeCommon)

set(CMAKE_CXX_STANDARD 17)

########### indi_libcamera_ccd ###########
set(indi_libcamera_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_libcamera.cpp
)

add_executable(indi_libcamera_ccd ${indi_libcamera_SRCS})

target_link_libraries(indi_libcamera_ccd 
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${LibCameraApps_LIBRARY}
    ${LibCamera_LIBRARY}
    ${Boost_LIBRARIES}
    ${USB1_LIBRARIES}
    ${LibRaw_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT})

#####################################

########### rpicam_test ###########
set(rpicam_test_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/rpicam_test.cpp
)

add_executable(rpicam_test ${rpicam_test_SRCS})

target_link_libraries(rpicam_test 
    ${LibCameraApps_LIBRARY}
    ${LibCamera_LIBRARY}
    ${Boost_LIBRARIES}
    ${USB1_LIBRARIES}
    ${LibRaw_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT})

#####################################

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_libcamera_ccd rt)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_libcamera_ccd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_libcamera.xml DESTINATION ${INDI_DATA_DIR})
