cmake_minimum_required(VERSION 3.16)
project(indi-icm-imu VERSION 1.0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)
find_package(ICM20948 REQUIRED)

set(ICM_VERSION_MAJOR 1)
set(ICM_VERSION_MINOR 0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

# Add your driver source files here
set(DRIVER_SRCS
    icm20948_imu.cpp
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${INDI_INCLUDE_DIR})

add_executable(indi_icm20948_imu ${DRIVER_SRCS})
target_link_libraries(indi_icm20948_imu PRIVATE ${INDI_LIBRARIES} ICM20948::icm20948)

# Install the driver
install(TARGETS indi_icm20948_imu RUNTIME DESTINATION bin)

# Install XML file
configure_file(indi_icm20948_imu.xml.cmake indi_icm20948_imu.xml @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_icm20948_imu.xml DESTINATION ${INDI_DATA_DIR})
