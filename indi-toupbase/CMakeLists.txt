cmake_minimum_required(VERSION 3.16)
PROJECT(indi_toupbase CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(TOUPCAM REQUIRED)
find_package(ALTAIRCAM REQUIRED)
find_package(BRESSERCAM REQUIRED)
find_package(MALLINCAM REQUIRED)
find_package(MEADECAM REQUIRED)
find_package(NNCAM REQUIRED)
find_package(OGMACAM REQUIRED)
find_package(OMEGONPROCAM REQUIRED)
find_package(STARSHOOTG REQUIRED)
find_package(TSCAM REQUIRED)
find_package(SVBONYCAM REQUIRED)

set(TOUPBASE_VERSION_MAJOR 2)
set(TOUPBASE_VERSION_MINOR 2)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${TOUPCAM_INCLUDE_DIR})
include_directories( ${ALTAIRCAM_INCLUDE_DIR})
include_directories( ${STARSHOOTG_INCLUDE_DIR})
include_directories( ${NNCAM_INCLUDE_DIR})
include_directories( ${MALLINCAM_INCLUDE_DIR})
include_directories( ${MEADECAM_INCLUDE_DIR})
include_directories( ${OMEGONPROCAM_INCLUDE_DIR})
include_directories( ${BRESSERCAM_INCLUDE_DIR})
include_directories( ${OGMACAM_INCLUDE_DIR})
include_directories( ${TSCAM_INCLUDE_DIR})
include_directories( ${SVBONYCAM_INCLUDE_DIR})

include(CMakeCommon)

set(indi_toupbase_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp)
set(indi_wheel_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupwheel.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp)
set(indi_focuser_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_focuser.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp)

########### indi_toupcam_* ###########
add_executable(indi_toupcam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_toupcam_ccd PRIVATE "-DBUILD_TOUPCAM")
target_link_libraries(indi_toupcam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_toupcam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_toupcam_wheel PRIVATE "-DBUILD_TOUPCAM")
target_link_libraries(indi_toupcam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_toupcam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_toupcam_focuser PRIVATE "-DBUILD_TOUPCAM")
target_link_libraries(indi_toupcam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_altair_* ###########
add_executable(indi_altair_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_altair_ccd PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(indi_altair_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_altair_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_altair_wheel PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(indi_altair_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_altair_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_altair_focuser PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(indi_altair_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_bressercam_* ###########
add_executable(indi_bressercam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_bressercam_ccd PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(indi_bressercam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${BRESSERCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_bressercam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_bressercam_wheel PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(indi_bressercam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${BRESSERCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_bressercam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_bressercam_focuser PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(indi_bressercam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${BRESSERCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_mallincam_* ###########
add_executable(indi_mallincam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_mallincam_ccd PRIVATE "-DBUILD_MALLINCAM")
target_link_libraries(indi_mallincam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MALLINCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_mallincam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_mallincam_wheel PRIVATE "-DBUILD_MALLINCAM")
target_link_libraries(indi_mallincam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MALLINCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_mallincam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_mallincam_focuser PRIVATE "-DBUILD_MALLINCAM")
target_link_libraries(indi_mallincam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MALLINCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_nncam_* ###########
add_executable(indi_nncam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_nncam_ccd PRIVATE "-DBUILD_NNCAM")
target_link_libraries(indi_nncam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${NNCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_nncam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_nncam_wheel PRIVATE "-DBUILD_NNCAM")
target_link_libraries(indi_nncam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${NNCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_nncam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_nncam_focuser PRIVATE "-DBUILD_NNCAM")
target_link_libraries(indi_nncam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${NNCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_ogmacam_* ###########
add_executable(indi_ogmacam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_ogmacam_ccd PRIVATE "-DBUILD_OGMACAM")
target_link_libraries(indi_ogmacam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OGMACAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_ogmacam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_ogmacam_wheel PRIVATE "-DBUILD_OGMACAM")
target_link_libraries(indi_ogmacam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OGMACAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_ogmacam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_ogmacam_focuser PRIVATE "-DBUILD_OGMACAM")
target_link_libraries(indi_ogmacam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OGMACAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_omegonprocam_* ###########
add_executable(indi_omegonprocam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_omegonprocam_ccd PRIVATE "-DBUILD_OMEGONPROCAM")
target_link_libraries(indi_omegonprocam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OMEGONPROCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_omegonprocam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_omegonprocam_wheel PRIVATE "-DBUILD_OMEGONPROCAM")
target_link_libraries(indi_omegonprocam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OMEGONPROCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_omegonprocam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_omegonprocam_focuser PRIVATE "-DBUILD_OMEGONPROCAM")
target_link_libraries(indi_omegonprocam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OMEGONPROCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_starshootg_* ###########
add_executable(indi_starshootg_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_starshootg_ccd PRIVATE "-DBUILD_STARSHOOTG")
target_link_libraries(indi_starshootg_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${STARSHOOTG_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_starshootg_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_starshootg_wheel PRIVATE "-DBUILD_STARSHOOTG")
target_link_libraries(indi_starshootg_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${STARSHOOTG_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_starshootg_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_starshootg_focuser PRIVATE "-DBUILD_STARSHOOTG")
target_link_libraries(indi_starshootg_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${STARSHOOTG_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_tscam_* ###########
add_executable(indi_tscam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_tscam_ccd PRIVATE "-DBUILD_TSCAM")
target_link_libraries(indi_tscam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TSCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_tscam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_tscam_wheel PRIVATE "-DBUILD_TSCAM")
target_link_libraries(indi_tscam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TSCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_tscam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_tscam_focuser PRIVATE "-DBUILD_TSCAM")
target_link_libraries(indi_tscam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TSCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_svbonycam_* ###########
add_executable(indi_svbonycam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_svbonycam_ccd PRIVATE "-DBUILD_SVBONYCAM")
target_link_libraries(indi_svbonycam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${SVBONYCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_svbonycam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_svbonycam_wheel PRIVATE "-DBUILD_SVBONYCAM")
target_link_libraries(indi_svbonycam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${SVBONYCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_svbonycam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_svbonycam_focuser PRIVATE "-DBUILD_SVBONYCAM")
target_link_libraries(indi_svbonycam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${SVBONYCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_meadecam_* ###########
add_executable(indi_meadecam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_meadecam_ccd PRIVATE "-DBUILD_MEADECAM")
target_link_libraries(indi_meadecam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MEADECAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_meadecam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_meadecam_wheel PRIVATE "-DBUILD_MEADECAM")
target_link_libraries(indi_meadecam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MEADECAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_meadecam_focuser ${indi_focuser_SRCS})
target_compile_definitions(indi_meadecam_focuser PRIVATE "-DBUILD_MEADECAM")
target_link_libraries(indi_meadecam_focuser ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MEADECAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

#####################################

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_toupcam_ccd rt)
target_link_libraries(indi_altair_ccd rt)
target_link_libraries(indi_bressercam_ccd rt)
target_link_libraries(indi_mallincam_ccd rt)
target_link_libraries(indi_nncam_ccd rt)
target_link_libraries(indi_ogmacam_ccd rt)
target_link_libraries(indi_omegonprocam_ccd rt)
target_link_libraries(indi_starshootg_ccd rt)
target_link_libraries(indi_tscam_ccd rt)
target_link_libraries(indi_svbonycam_ccd rt)
target_link_libraries(indi_meadecam_ccd rt)
target_link_libraries(indi_toupcam_wheel rt)
target_link_libraries(indi_altair_wheel rt)
target_link_libraries(indi_bressercam_wheel rt)
target_link_libraries(indi_mallincam_wheel rt)
target_link_libraries(indi_nncam_wheel rt)
target_link_libraries(indi_ogmacam_wheel rt)
target_link_libraries(indi_omegonprocam_wheel rt)
target_link_libraries(indi_starshootg_wheel rt)
target_link_libraries(indi_tscam_wheel rt)
target_link_libraries(indi_svbonycam_wheel rt)
target_link_libraries(indi_meadecam_wheel rt)
target_link_libraries(indi_toupcam_focuser rt)
target_link_libraries(indi_altair_focuser rt)
target_link_libraries(indi_bressercam_focuser rt)
target_link_libraries(indi_mallincam_focuser rt)
target_link_libraries(indi_nncam_focuser rt)
target_link_libraries(indi_ogmacam_focuser rt)
target_link_libraries(indi_omegonprocam_focuser rt)
target_link_libraries(indi_starshootg_focuser rt)
target_link_libraries(indi_tscam_focuser rt)
target_link_libraries(indi_svbonycam_focuser rt)
target_link_libraries(indi_meadecam_focuser rt)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS
    indi_toupcam_ccd
    indi_altair_ccd
    indi_bressercam_ccd
    indi_mallincam_ccd
    indi_nncam_ccd
    indi_ogmacam_ccd
    indi_omegonprocam_ccd
    indi_starshootg_ccd
    indi_tscam_ccd
    indi_svbonycam_ccd
    indi_meadecam_ccd
    indi_toupcam_wheel
    indi_altair_wheel
    indi_bressercam_wheel
    indi_mallincam_wheel
    indi_nncam_wheel
    indi_ogmacam_wheel
    indi_omegonprocam_wheel
    indi_starshootg_wheel
    indi_tscam_wheel
    indi_svbonycam_wheel
    indi_meadecam_wheel
    indi_toupcam_focuser
    indi_altair_focuser
    indi_bressercam_focuser
    indi_mallincam_focuser
    indi_nncam_focuser
    indi_ogmacam_focuser
    indi_omegonprocam_focuser
    indi_starshootg_focuser
    indi_tscam_focuser
    indi_svbonycam_focuser
    indi_meadecam_focuser
    RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml DESTINATION ${INDI_DATA_DIR})
