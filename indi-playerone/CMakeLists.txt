cmake_minimum_required(VERSION 3.0)
PROJECT(indi_playerone CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(PLAYERONE REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

set(PLAYERONE_VERSION_MAJOR 1)
set(PLAYERONE_VERSION_MINOR 8)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_playerone.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_playerone.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${PLAYERONE_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

if (INDI_WEBSOCKET)
    find_package(websocketpp REQUIRED)
    find_package(Boost COMPONENTS system thread)
    include_directories (${Boost_INCLUDE_DIRS})
    add_definitions(-DHAVE_WEBSOCKET)
    SET(HAVE_WEBSOCKET 1)
endif()

########### indi_playerone_ccd ###########
set(indi_playerone_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/playerone_base.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/playerone_ccd.cpp
   )

add_executable(indi_playerone_ccd ${indi_playerone_SRCS})
target_link_libraries(indi_playerone_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${PLAYERONE_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (HAVE_WEBSOCKET)
    target_link_libraries(indi_playerone_ccd ${Boost_LIBRARIES})
endif()

########### indi_playerone_single_ccd ###########
set(indi_playerone_single_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/playerone_base.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/playerone_single_ccd.cpp
   )

add_executable(indi_playerone_single_ccd ${indi_playerone_single_SRCS})
target_link_libraries(indi_playerone_single_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${PLAYERONE_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (HAVE_WEBSOCKET)
    target_link_libraries(indi_playerone_single_ccd ${Boost_LIBRARIES})
endif()

########### indi_playerone_wheel ###########
add_executable(indi_playerone_wheel ${CMAKE_CURRENT_SOURCE_DIR}/playerone_wheel.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_playerone_wheel ${INDI_LIBRARIES} ${PLAYERONE_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_playerone_wheel ${INDI_LIBRARIES} ${PLAYERONE_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### playerone_camera_test ###########
add_executable(playerone_camera_test ${CMAKE_CURRENT_SOURCE_DIR}/playerone_camera_test.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(playerone_camera_test ${PLAYERONE_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(playerone_camera_test ${PLAYERONE_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_playerone_ccd rt)
target_link_libraries(indi_playerone_single_ccd rt)
target_link_libraries(indi_playerone_wheel rt)
target_link_libraries(playerone_camera_test rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_playerone_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_playerone_single_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_playerone_wheel RUNTIME DESTINATION bin)
install(TARGETS playerone_camera_test RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_playerone.xml DESTINATION ${INDI_DATA_DIR})
