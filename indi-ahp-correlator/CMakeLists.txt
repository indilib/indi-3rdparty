cmake_minimum_required(VERSION 2.7)
PROJECT(indi_ahp_correlator CXX C)

set (AHP_CORRELATOR_VERSION_MAJOR 1)
set (AHP_CORRELATOR_VERSION_MINOR 0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(AHPXC REQUIRED)
find_package(Nova REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_ahp_correlator.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_ahp_correlator.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${AHPXC_INCLUDE_DIR})

include(CMakeCommon)

############# AHP_CORRELATOR CCD ###############
if (CFITSIO_FOUND)

set(AHP_CORRELATOR_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indi_ahp_correlator.cpp
)

add_executable(indi_ahp_correlator ${AHP_CORRELATOR_SRCS})

target_link_libraries(indi_ahp_correlator ${INDI_LIBRARIES} ${AHPXC_LIBRARIES} ${NOVA_LIBRARIES} ${CFITSIO_LIBRARIES} ${M_LIB} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_ahp_correlator RUNTIME DESTINATION bin)

endif (CFITSIO_FOUND)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_ahp_correlator.xml DESTINATION ${INDI_DATA_DIR})
