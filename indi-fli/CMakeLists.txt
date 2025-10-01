cmake_minimum_required(VERSION 3.16)
PROJECT(indi_fli C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(FLI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(FLIPRO)

set (FLI_CCD_VERSION_MAJOR 2)
set (FLI_CCD_VERSION_MINOR 0)

OPTION (LEGACY_MODE "Build Legacy Mode" OFF)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_fli.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_fli.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${FLI_INCLUDE_DIR})

include(CMakeCommon)

IF (LEGACY_MODE)
# Legacy mode to support older INDI properties used in some observatories.
add_definitions(-DLEGACY_MODE)
ENDIF(LEGACY_MODE)

###### FLI Precision Digital Focuser ######
set(flipdf_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/fli_pdf.cpp
)

add_executable(indi_fli_focus ${flipdf_SRCS})

target_link_libraries(indi_fli_focus ${INDI_LIBRARIES} ${FLI_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_fli_focus RUNTIME DESTINATION bin )

###### FLI Filter wheel ######
set(flicfw_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/fli_cfw.cpp
)

add_executable(indi_fli_wheel ${flicfw_SRCS})

target_link_libraries(indi_fli_wheel ${INDI_LIBRARIES} ${FLI_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_fli_wheel RUNTIME DESTINATION bin )

############# FLI CCD ###############
set(fliccd_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/fli_ccd.cpp
)

add_executable(indi_fli_ccd ${fliccd_SRCS})

target_link_libraries(indi_fli_ccd ${INDI_LIBRARIES} ${FLI_LIBRARIES} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_fli_ccd RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_fli.xml DESTINATION ${INDI_DATA_DIR})

if (FLIPRO_FOUND)
include_directories( ${FLIPRO_INCLUDE_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_flipro.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_flipro.xml )

# Check GLIBC version for Kepler camera
execute_process(COMMAND getconf GNU_LIBC_VERSION OUTPUT_VARIABLE GLIBC_VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)

string(REGEX MATCH "([0-9]+)\\.([0-9]+)" GLIBC_VERSION_MATCH "${GLIBC_VERSION_STRING}")
if(GLIBC_VERSION_MATCH)
    set(GLIBC_MAJOR_VERSION "${CMAKE_MATCH_1}")
    set(GLIBC_MINOR_VERSION "${CMAKE_MATCH_2}")
else()
    message(WARNING "Could not determine GLIBC version. Assuming compatible for Kepler camera.")
    set(GLIBC_MAJOR_VERSION 999) # Assume high version if cannot determine
    set(GLIBC_MINOR_VERSION 999)
endif()

if(${GLIBC_MAJOR_VERSION} GREATER_EQUAL 2 AND ${GLIBC_MINOR_VERSION} GREATER_EQUAL 34)
    message(STATUS "GLIBC version ${GLIBC_MAJOR_VERSION}.${GLIBC_MINOR_VERSION} found. Building Kepler camera.")

    ############# Kepler Camera ###############
    set(kepler_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/kepler.cpp
    )

    add_executable(indi_kepler_ccd ${kepler_SRCS})

    target_link_libraries(indi_kepler_ccd ${INDI_LIBRARIES} ${FLIPRO_LIBRARIES} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

    install(TARGETS indi_kepler_ccd RUNTIME DESTINATION bin)

    add_executable(test_kepler_merged test_kepler_merged.cpp)
    target_link_libraries(test_kepler_merged ${FLIPRO_LIBRARIES} ${CFITSIO_LIBRARIES} usb-1.0)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_flipro.xml DESTINATION ${INDI_DATA_DIR})
else()
    message(WARNING "GLIBC version ${GLIBC_MAJOR_VERSION}.${GLIBC_MINOR_VERSION} is too low. Kepler camera requires GLIBC 2.34 or higher. Skipping build of Kepler camera.")
endif()

endif (FLIPRO_FOUND)
