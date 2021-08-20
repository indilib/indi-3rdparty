cmake_minimum_required(VERSION 3.0)
project(indi-bresserexos2 VERSION 0.901)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_bresserexos2.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_bresserexos2.xml )

include(CMakeCommon)

find_package(Threads REQUIRED)
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

add_executable(indi_bresserexos2 BresserExosIIGoToDriver.cpp IndiSerialWrapper.cpp SerialCommand.cpp)
target_link_libraries(indi_bresserexos2 ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} Threads::Threads)
target_include_directories(indi_bresserexos2 PUBLIC
						  "${PROJECT_BINARY_DIR}"
						  )

install(TARGETS indi_bresserexos2 DESTINATION bin)
install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_bresserexos2.xml DESTINATION ${INDI_DATA_DIR})
