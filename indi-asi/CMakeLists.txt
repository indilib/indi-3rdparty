cmake_minimum_required(VERSION 3.16)
PROJECT(indi_asi CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(ASI REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

set(ASI_VERSION_MAJOR 2)
set(ASI_VERSION_MINOR 5)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_asi.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_asi.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${ASI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

########### indi_asi_ccd ###########
set(indi_asi_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_base.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/usb_utils.cpp
   )

add_executable(indi_asi_ccd ${indi_asi_SRCS})
target_link_libraries(indi_asi_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

########### indi_asi_single_ccd ###########
set(indi_asi_single_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_base.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_single_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/usb_utils.cpp
   )

add_executable(indi_asi_single_ccd ${indi_asi_single_SRCS})
target_link_libraries(indi_asi_single_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

########### indi_asi_wheel ###########
add_executable(indi_asi_wheel ${CMAKE_CURRENT_SOURCE_DIR}/asi_wheel.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_wheel ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_wheel ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### indi_asi_st4 ###########
add_executable(indi_asi_st4 ${CMAKE_CURRENT_SOURCE_DIR}/asi_st4.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_st4 ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_st4 ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### indi_asi_focuser ###########
add_executable(indi_asi_focuser ${CMAKE_CURRENT_SOURCE_DIR}/asi_focuser.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_focuser ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_focuser ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### indi_asi_rotator ###########
add_executable(indi_asi_rotator ${CMAKE_CURRENT_SOURCE_DIR}/asi_rotator.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_rotator ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_rotator ${INDI_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### asi_camera_test ###########
add_executable(asi_camera_test ${CMAKE_CURRENT_SOURCE_DIR}/asi_camera_test.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(asi_camera_test ${ASI_LIBRARIES} ${USB1_LIBRARIES})
ELSE()
target_link_libraries(asi_camera_test ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

#####################################

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_asi_ccd rt)
target_link_libraries(indi_asi_single_ccd rt)
target_link_libraries(indi_asi_wheel rt)
target_link_libraries(indi_asi_st4 rt)
target_link_libraries(indi_asi_focuser rt)
target_link_libraries(indi_asi_rotator rt)
target_link_libraries(asi_camera_test rt)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_asi_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_asi_single_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_asi_wheel RUNTIME DESTINATION bin)
install(TARGETS indi_asi_st4 RUNTIME DESTINATION bin)
install(TARGETS indi_asi_focuser RUNTIME DESTINATION bin)
install(TARGETS indi_asi_rotator RUNTIME DESTINATION bin)
install(TARGETS asi_camera_test RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_asi.xml DESTINATION ${INDI_DATA_DIR})

########### test-usb-utils ###########
add_executable(test-usb-utils ${CMAKE_CURRENT_SOURCE_DIR}/test-usb-utils.cpp ${CMAKE_CURRENT_SOURCE_DIR}/usb_utils.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
ENDIF()
target_link_libraries(test-usb-utils ${INDI_LIBRARIES} ${USB1_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(test-usb-utils rt)
endif()

########### force_usb_reset ###########
add_executable(force_usb_reset ${CMAKE_CURRENT_SOURCE_DIR}/force_usb_reset.cpp ${CMAKE_CURRENT_SOURCE_DIR}/usb_utils.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
ENDIF()
target_link_libraries(force_usb_reset ${INDI_LIBRARIES} ${USB1_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(force_usb_reset rt)
endif()
install(TARGETS force_usb_reset RUNTIME DESTINATION bin)
