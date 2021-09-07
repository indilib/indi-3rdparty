cmake_minimum_required(VERSION 3.0)
PROJECT(indi-asi-power CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 95)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
find_package(RT REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_asi_power.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_asi_power.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

include(CMakeCommon)

set(GPIO_LIBRARIES "pigpiod_if2.so")

################ ASI Power ################
set(indi_asi_power_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/asipower.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indi_asi_power indi_asi_power_SRCS 6 cpp)
ENDIF ()

add_compile_options(-Wall)

add_executable(indi_asi_power ${indi_asi_power_SRCS})
target_link_libraries(indi_asi_power ${INDI_LIBRARIES} ${GPIO_LIBRARIES})

# Install indi_asi_power
install(TARGETS indi_asi_power RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_asi_power.xml DESTINATION ${INDI_DATA_DIR})
