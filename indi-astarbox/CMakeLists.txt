cmake_minimum_required(VERSION 3.16)
PROJECT(indi-astarbox CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set (VERSION_MAJOR 1)
set (VERSION_MINOR 0)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
find_package(RT REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_astarbox.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_astarbox.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

include(CMakeCommon)

################ ASI Power ################
set(indi_astarbox_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indi-astarbox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AStarBox.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mcp3421.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PCA9685.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indi_astarbox indi_astarbox_SRCS 6 cpp)
ENDIF ()

add_compile_options(-Wall)

add_executable(indi_astarbox ${indi_astarbox_SRCS})
target_link_libraries(indi_astarbox ${INDI_LIBRARIES})

# Install indi_astarbox
install(TARGETS indi_astarbox RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_astarbox.xml DESTINATION ${INDI_DATA_DIR})
