cmake_minimum_required(VERSION 2.8)
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

set(ASI_VERSION_MAJOR 1)
set(ASI_VERSION_MINOR 9)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_asi.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_asi.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${ASI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

if (INDI_WEBSOCKET)
    find_package(websocketpp REQUIRED)
    find_package(Boost COMPONENTS system thread)
    include_directories (${Boost_INCLUDE_DIRS})
    add_definitions(-DHAVE_WEBSOCKET)
    SET(HAVE_WEBSOCKET 1)
endif()

########### indi_asi_ccd ###########
set(indi_asi_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_ccd.cpp
   )

add_executable(indi_asi_ccd ${indi_asi_SRCS})
target_link_libraries(indi_asi_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (HAVE_WEBSOCKET)
    target_link_libraries(indi_asi_ccd ${Boost_LIBRARIES})
endif()

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

########### asi_camera_test ###########
add_executable(asi_camera_test ${CMAKE_CURRENT_SOURCE_DIR}/asi_camera_test.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(asi_camera_test ${ASI_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(asi_camera_test ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_asi_ccd rt)
target_link_libraries(indi_asi_wheel rt)
target_link_libraries(indi_asi_st4 rt)
target_link_libraries(indi_asi_focuser rt)
target_link_libraries(asi_camera_test rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_asi_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_asi_wheel RUNTIME DESTINATION bin)
install(TARGETS indi_asi_st4 RUNTIME DESTINATION bin)
install(TARGETS indi_asi_focuser RUNTIME DESTINATION bin)
install(TARGETS asi_camera_test RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_asi.xml DESTINATION ${INDI_DATA_DIR})
