cmake_minimum_required(VERSION 3.10)
project(indi_startech_hub VERSION 1.0.0 LANGUAGES CXX)

# Allow building this driver both as part of indi-3rdparty and standalone from this folder.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules")

set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VERSION_MINOR ${PROJECT_VERSION_MINOR})

find_package(INDI REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_startech_hub.xml.in
               ${CMAKE_CURRENT_BINARY_DIR}/indi_startech_hub.xml @ONLY)

add_executable(indi_startech_hub
  indi_startech_hub.cpp
)

target_include_directories(indi_startech_hub PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${INDI_INCLUDE_DIR}
)

target_link_libraries(indi_startech_hub PRIVATE
  ${INDI_LIBRARIES}
)

install(TARGETS indi_startech_hub RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_startech_hub.xml
        DESTINATION ${INDI_DATA_DIR}/drivers)
