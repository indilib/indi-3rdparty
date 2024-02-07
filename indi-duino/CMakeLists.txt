cmake_minimum_required(VERSION 3.16)
PROJECT(indi_duino CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
set(FIRMATA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/")
include(GNUInstallDirs)

set (DUINO_VERSION_MAJOR 0)
set (DUINO_VERSION_MINOR 6)
 
set (WEATHERRADIO_VERSION_MAJOR 1)
set (WEATHERRADIO_VERSION_MINOR 17)

set (DUINOPOWERBOX_VERSION_MAJOR 0)
set (DUINOPOWERBOX_VERSION_MINOR 1)

find_package(INDI REQUIRED)
find_package(CURL REQUIRED)

if (CMAKE_VERSION VERSION_LESS 3.12.0)
set(CURL ${CURL_LIBRARIES})
else()
set(CURL CURL::libcurl)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_duino.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_duino.xml )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_weatherradio.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_weatherradio.xml )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${FIRMATA_INCLUDE_DIR})

include(CMakeCommon)

################# libfirmata ############################
set (firmata_VERSION_MAJOR 1)
set (firmata_VERSION_MINOR 0)
set (firmata_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/firmata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/arduino.cpp
)
SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/firmata.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/arduino.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
add_library(firmata STATIC ${firmata_SRCS})
SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/blink_pin.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
add_executable(blink_pin ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/blink_pin.cpp)
target_link_libraries (blink_pin firmata)
add_executable(read_digital_pin ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/read_digital_pin.cpp)
target_link_libraries (read_digital_pin firmata)
add_executable(read_analog_pin ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/read_analog_pin.cpp)
target_link_libraries (read_analog_pin firmata)
add_executable(ask_digital_pin ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/ask_digital_pin.cpp)
target_link_libraries (ask_digital_pin firmata)
add_executable(ask_analog_pin ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/ask_analog_pin.cpp)
target_link_libraries (ask_analog_pin firmata)
add_executable(read_string ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/examples/read_string.cpp)
target_link_libraries (read_string firmata)

##################### indi arduino #####################
set(indiduino_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indiduino.cpp
   )

add_executable(indi_duino ${indiduino_SRCS})
target_link_libraries(indi_duino ${INDI_LIBRARIES} firmata)

install(TARGETS indi_duino RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_duino.xml DESTINATION ${INDI_DATA_DIR})
if (APPLE)
   install(TARGETS firmata LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()
##################### weather radio #####################
SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_SOURCE_DIR}/gason/gason.cpp PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough")
set(weatherradio_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/gason/gason.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/weatherradio.cpp
   )

add_executable(indi_weatherradio ${weatherradio_SRCS})
target_link_libraries(indi_weatherradio ${INDI_LIBRARIES} ${CURL})

install(TARGETS indi_weatherradio RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_weatherradio.xml DESTINATION ${INDI_DATA_DIR})
##################### power box ######################
set(powerbox_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/gason/gason.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/powerbox.cpp
   )

add_executable(indi_duino_powerbox ${powerbox_SRCS})
target_link_libraries(indi_duino_powerbox ${INDI_LIBRARIES} ${CURL})

install(TARGETS indi_duino_powerbox RUNTIME DESTINATION bin)
################### DEVICES XML  #####################
add_subdirectory(devices)

