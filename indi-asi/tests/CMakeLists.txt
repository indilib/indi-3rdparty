cmake_minimum_required(VERSION 3.16)

PROJECT(indi_asi_tests CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")

find_package(INDI REQUIRED)
find_package(ASI REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

include_directories(${INDI_INCLUDE_DIR})
include_directories(${ASI_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(asi_stress_client asi_stress_client.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(asi_stress_client ${INDI_LIBRARIES} indiclient ${ASI_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(asi_stress_client ${INDI_LIBRARIES} indiclient ${ASI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(asi_stress_client rt)
endif()

install(TARGETS asi_stress_client RUNTIME DESTINATION bin)

########### test-usb-utils ###########
add_executable(test-usb-utils test-usb-utils.cpp ../usb_utils.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
ENDIF()
target_link_libraries(test-usb-utils ${INDI_LIBRARIES} ${USB1_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(test-usb-utils rt)
endif()

########### force_usb_reset ###########
add_executable(force_usb_reset force_usb_reset.cpp ../usb_utils.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
ENDIF()
target_link_libraries(force_usb_reset ${INDI_LIBRARIES} indiclient ${USB1_LIBRARIES})
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(force_usb_reset rt)
endif()
install(TARGETS force_usb_reset RUNTIME DESTINATION bin)
