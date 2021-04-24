cmake_minimum_required(VERSION 3.13)
project(indi-bresserexos2 VERSION 0.822)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(XML_INSTALL_DIR /usr/share/indi)
set(CMAKE_INSTALL_PREFIX /usr/bin)

configure_file(Config.hpp.in Config.hpp)
configure_file(indi_bresserexos2.xml.in indi_bresserexos2.xml)

find_package(Threads REQUIRED)
find_package(INDI REQUIRED)

include_directories(${PROJECT_BINARY_DIR})

option(USE_CERR_LOGGING "log error messages using std::cerr for debugging" ON)

add_subdirectory(SerialDeviceControl)

add_executable(indi_bresserexos2 BresserExosIIGoToDriver.cpp IndiSerialWrapper.cpp)
target_link_libraries(indi_bresserexos2 indidriver SerialDeviceControl ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(indi_bresserexos2 PUBLIC
						  "${PROJECT_BINARY_DIR}"
						  "${PROJECT_SOURCE_DIR}/SerialDeviceControl"
						  
						  )
						  
include(GNUInstallDirs)
install(TARGETS indi_bresserexos2 DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_BINARY_DIR}/indi_bresserexos2.xml DESTINATION ${XML_INSTALL_DIR})
