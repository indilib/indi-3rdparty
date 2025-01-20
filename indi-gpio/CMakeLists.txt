cmake_minimum_required(VERSION 3.16)
PROJECT(indi-gpio CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 2)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
find_package(GPIOD REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_gpio.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_gpio.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${GPIOD_INCLUDE_DIR})

include(CMakeCommon)

set(CMAKE_CXX_STANDARD 17)

################ GPIO ################
set(indi_gpio_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indi_gpio.cpp
   )

add_executable(indi_gpio ${indi_gpio_SRCS})
target_link_libraries(indi_gpio ${INDI_LIBRARIES} ${GPIOD_LIBRARIES})

# Install
install(TARGETS indi_gpio RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_gpio.xml DESTINATION ${INDI_DATA_DIR})
