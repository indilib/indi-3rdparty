cmake_minimum_required(VERSION 3.0)
PROJECT(aagcloudwatcher CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set (AAG_VERSION_MAJOR 1)
set (AAG_VERSION_MINOR 5)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_aagcloudwatcher_ng.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_aagcloudwatcher_ng.xml)

include(CMakeCommon)

set(indiaag_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_aagcloudwatcher_ng.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CloudWatcherController_ng.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indiaag indiaag_SRCS 10 cpp)
ENDIF ()

add_executable(indi_aagcloudwatcher_ng ${indiaag_SRCS})
target_link_libraries(indi_aagcloudwatcher_ng ${INDI_LIBRARIES})

set(test_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CloudWatcherController_ng.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(test test_SRCS 10 cpp)
ENDIF ()

add_executable(aagcloudwatcher_test_ng ${test_SRCS})
target_link_libraries(aagcloudwatcher_test_ng ${INDI_LIBRARIES})

install(TARGETS indi_aagcloudwatcher_ng RUNTIME DESTINATION bin)
install(TARGETS aagcloudwatcher_test_ng RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_aagcloudwatcher_ng.xml DESTINATION ${INDI_DATA_DIR})
install(FILES indi_aagcloudwatcher_ng_sk.xml DESTINATION ${INDI_DATA_DIR})
