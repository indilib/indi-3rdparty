cmake_minimum_required(VERSION 3.0)
PROJECT(indi_libcamera CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)
#find_package(LibCameraApps REQUIRED)
find_package(LibRaw REQUIRED)
find_package(JPEG REQUIRED)
find_package(Boost COMPONENTS program_options)
find_package(PkgConfig REQUIRED)
find_library(EXIF_LIBRARY exif REQUIRED)
find_library(TIFF_LIBRARY tiff REQUIRED)
find_library(DRM_LIBRARY drm REQUIRED)


pkg_check_modules(LIBCAMERA REQUIRED libcamera)
message(STATUS "libcamera library found:")
message(STATUS "    version: ${LIBCAMERA_VERSION}")
message(STATUS "    libraries: ${LIBCAMERA_LINK_LIBRARIES}")
message(STATUS "    include path: ${LIBCAMERA_INCLUDE_DIRS}")

set(LIBCAMERA_VERSION_MAJOR 1)
set(LIBCAMERA_VERSION_MINOR 0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_libcamera.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_libcamera.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${LibRaw_INCLUDE_DIR})
include_directories( ${LIBCAMERA_INCLUDE_DIRS})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( libcamera-apps)

include(CMakeCommon)

set (CMAKE_CXX_STANDARD 17)

########### indi_libcamera_ccd ###########
set(indi_libcamera_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_libcamera.cpp
)


set(LIBCAMERAAPPS_APPS libcamera_app)
set(LIBCAMERAAPPS_ENCODERS encoders)
set(LIBCAMERAAPPS_IMAGES images)
set(LIBCAMERAAPPS_OUTPUTS outputs)
set(LIBCAMERAAPPS_PREVIEW preview)
set(LIBCAMERAAPPS_POST post_processing_stages)

add_subdirectory(libcamera-apps SYSTEM)
add_executable(indi_libcamera_ccd ${indi_libcamera_SRCS})

target_link_libraries(indi_libcamera_ccd 
    ${INDI_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${LIBCAMERAAPPS_ENCODERS}
    ${LIBCAMERAAPPS_APPS}
    ${LIBCAMERAAPPS_IMAGES}
    ${LIBCAMERAAPPS_OUTPUTS}
    ${LIBCAMERAAPPS_POST}
    ${LIBCAMERAAPPS_PREVIEW}
    ${Boost_LIBRARIES}
    ${USB1_LIBRARIES}
    ${LibRaw_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${LIBCAMERA_LINK_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT})

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_libcamera_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_libcamera_ccd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_libcamera.xml DESTINATION ${INDI_DATA_DIR})
