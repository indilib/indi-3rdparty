cmake_minimum_required(VERSION 2.8.2)

project(cli_sample CXX C)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

include_directories(
  ../../include)

if("${CMAKE_LIBRARY_ARCHITECTURE}" MATCHES "^(x86_64|amd64)")
  set(architecture_path "x64")
elseif("${CMAKE_LIBRARY_ARCHITECTURE}" MATCHES "^i[3-6]86")
  set(architecture_path "x86")
elseif("${CMAKE_LIBRARY_ARCHITECTURE}" MATCHES "^arm-linux-gnueabihf")
  set(architecture_path "armv7l")
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_LIBRARY_ARCHITECTURE}")
endif()
message(STATUS "${architecture_path}")

link_directories(
  ${PROJECT_SOURCE_DIR}/../../lib/${architecture_path})

add_executable(cli cli.cpp)

set(CMAKE_SKIP_RPATH TRUE)

target_link_libraries(cli
  RicohCameraSDKCpp
  mtp
  pthread)
