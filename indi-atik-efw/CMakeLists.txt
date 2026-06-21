cmake_minimum_required(VERSION 3.16)
PROJECT(indi_atik_efw CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

set(ATIK_EFW_VERSION_MAJOR 1)
set(ATIK_EFW_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_atik_efw.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_atik_efw.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${USB1_INCLUDE_DIRS})

include(CMakeCommon)

set(indi_atik_efw_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/atik_efw.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/atik_efw_usb.cpp
   )

add_executable(indi_atik_efw ${indi_atik_efw_SRCS})
target_link_libraries(indi_atik_efw ${INDI_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
IF (APPLE)
target_link_libraries(indi_atik_efw "-framework IOKit" "-framework CoreFoundation")
ENDIF()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_atik_efw rt)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_atik_efw RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_atik_efw.xml DESTINATION ${INDI_DATA_DIR})

IF (INDI_BUILD_UNITTESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  set(ATIK_EFW_TEST_SRCS
     ${CMAKE_CURRENT_SOURCE_DIR}/atik_efw.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/atik_efw_usb.cpp
     )

  add_executable(atik_efw_test ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/test_atik_efw.cpp ${ATIK_EFW_TEST_SRCS})
  target_compile_definitions(atik_efw_test PRIVATE ATIK_EFW_DISABLE_LOADER)
  target_link_libraries(atik_efw_test ${INDI_LIBRARIES} ${USB1_LIBRARIES} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME atik_efw_test COMMAND atik_efw_test)
ENDIF (INDI_BUILD_UNITTESTS)
