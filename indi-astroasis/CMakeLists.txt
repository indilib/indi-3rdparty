cmake_minimum_required(VERSION 3.0)
PROJECT(indi_astroasis CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(ASTROASIS REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

set(ASTROASIS_VERSION_MAJOR 1)
set(ASTROASIS_VERSION_MINOR 0)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_astroasis.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_astroasis.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${ASTROASIS_INCLUDE_DIR})

include(CMakeCommon)

########### indi_oasis_focuser ###########
add_executable(indi_oasis_focuser ${CMAKE_CURRENT_SOURCE_DIR}/oasis_focuser.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_oasis_focuser ${INDI_LIBRARIES} ${ASTROASIS_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_oasis_focuser ${INDI_LIBRARIES} ${ASTROASIS_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_oasis_focuser rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_oasis_focuser RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_astroasis.xml DESTINATION ${INDI_DATA_DIR})
