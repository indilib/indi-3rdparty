# define the project name
project(indi-hakos-roof C CXX)
cmake_minimum_required(VERSION 3.1)

# INDI 2.1.9 requires C++17 due to std::any usage in headers
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force C++17 for all compilation and includes
add_compile_options(-std=c++17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

include(GNUInstallDirs)

# add our cmake_modules folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")

# find our required packages
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GSL REQUIRED)
find_package(CURL REQUIRED)

# these will be used to set the version number in config.h and our driver's xml file
set(CDRIVER_VERSION_MAJOR 1)
set(CDRIVER_VERSION_MINOR 1)

# do the replacement in the config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# do the replacement in the driver's xml file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_hakos_roof.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_hakos_roof.xml
)

# set our include directories to look for header files
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${EV_INCLUDE_DIR})

include(CMakeCommon)
# Clang Format support
IF (UNIX OR APPLE)
    SET(FORMAT_CODE OFF CACHE BOOL "Enable Clang Format")
    IF (FORMAT_CODE MATCHES ON)
        FILE(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

        FOREACH(SOURCE_FILE ${ALL_SOURCE_FILES})
            STRING(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)
            IF (NOT ${DIR_FOUND} EQUAL 0)
                LIST(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            ENDIF ()
        ENDFOREACH ()

        FIND_PROGRAM(CLANGFORMAT_EXE NAMES clang-format-5.0)
        IF (CLANGFORMAT_EXE)
            ADD_CUSTOM_TARGET(clang-format COMMAND ${CLANGFORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES})
        ENDIF ()
    ENDIF ()
ENDIF ()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.3.9)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-overflow=2")
ENDIF ()


# tell cmake to build our executable
add_executable(
    indi_hakos_roof
    indi_hakos_roof.cpp
)

# and link it to these libraries
target_link_libraries(
    indi_hakos_roof
    ${INDI_LIBRARIES}
    ${NOVA_LIBRARIES}
    ${GSL_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    CURL::libcurl

)

# tell cmake where to install our executable
install(TARGETS indi_hakos_roof RUNTIME DESTINATION bin)

# and where to put the driver's xml file.
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/indi_hakos_roof.xml
    DESTINATION ${INDI_DATA_DIR}
)
