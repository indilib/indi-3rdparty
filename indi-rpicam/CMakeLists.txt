cmake_minimum_required(VERSION 3.0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

PROJECT(indi_rpicam CXX C)

set(INDI_RPICAM_VERSION_MAJOR 1)
set(INDI_RPICAM_VERSION_MINOR 1)

include(GNUInstallDirs)
include(CMakeCommon)

find_package(MMAL REQUIRED)
find_package(INDI REQUIRED)
find_package(CFITSIO REQUIRED)

include_directories(${INDI_INCLUDE_DIR})
include_directories(${MMAL_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})

find_package(Threads REQUIRED)

set(INDI_RPICAM_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/mmalcamera.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_rpicam.cpp
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

add_executable(indi_rpicam ${INDI_RPICAM_SRCS})



target_link_libraries(indi_rpicam
    ${INDI_DRIVER_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${INDI_LIBRARIES}
    ${MMAL_LIBRARIES}
    ${Threads_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_rpicam.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_rpicam.xml )

install(TARGETS indi_rpicam RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_rpicam.xml DESTINATION ${INDI_DATA_DIR})
