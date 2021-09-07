cmake_minimum_required(VERSION 3.0)
PROJECT(indi-starbook-ten CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

include(CMakeCommon)

set(STARBOOK_TEN_VERSION_MAJOR 0)
set(STARBOOK_TEN_VERSION_MINOR 1)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook_ten.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_starbook_ten.xml)


########### Starbook Ten ###########
set(indi_starbook_ten_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/connectionhttp.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook_ten.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/starbook_ten.cpp
   )

add_executable(indi_starbook_ten ${indi_starbook_ten_SRCS})
target_link_libraries(indi_starbook_ten ${INDI_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_starbook_ten RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_starbook_ten.xml DESTINATION ${INDI_DATA_DIR})
