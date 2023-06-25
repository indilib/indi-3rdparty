cmake_minimum_required(VERSION 3.0)
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
find_package(NNCAM REQUIRED)
find_package(OGMACAM REQUIRED)
find_package(OMEGONPROCAM REQUIRED)
find_package(STARSHOOTG REQUIRED)
find_package(TSCAM REQUIRED)

set(TOUPBASE_VERSION_MAJOR 2)
set(TOUPBASE_VERSION_MINOR 1)

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
include_directories( ${OMEGONPROCAM_INCLUDE_DIR})
include_directories( ${BRESSERCAM_INCLUDE_DIR})
include_directories( ${OGMACAM_INCLUDE_DIR})
include_directories( ${TSCAM_INCLUDE_DIR})

include(CMakeCommon)

set(indi_toupbase_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp)
set(indi_wheel_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupwheel.cpp ${CMAKE_CURRENT_SOURCE_DIR}/libtoupbase.cpp)

########### indi_toupcam_* ###########
add_executable(indi_toupcam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_toupcam_ccd PRIVATE "-DBUILD_TOUPCAM")
target_link_libraries(indi_toupcam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_toupcam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_toupcam_wheel PRIVATE "-DBUILD_TOUPCAM")
target_link_libraries(indi_toupcam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_altair_* ###########
add_executable(indi_altair_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_altair_ccd PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(indi_altair_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_altair_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_altair_wheel PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(indi_altair_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_bressercam_* ###########
add_executable(indi_bressercam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_bressercam_ccd PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(indi_bressercam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${BRESSERCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_bressercam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_bressercam_wheel PRIVATE "-DBUILD_BRESSERCAM")
target_link_libraries(indi_bressercam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${BRESSERCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_mallincam_* ###########
add_executable(indi_mallincam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_mallincam_ccd PRIVATE "-DBUILD_MALLINCAM")
target_link_libraries(indi_mallincam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MALLINCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_mallincam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_mallincam_wheel PRIVATE "-DBUILD_MALLINCAM")
target_link_libraries(indi_mallincam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${MALLINCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_nncam_* ###########
add_executable(indi_nncam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_nncam_ccd PRIVATE "-DBUILD_NNCAM")
target_link_libraries(indi_nncam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${NNCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_nncam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_nncam_wheel PRIVATE "-DBUILD_NNCAM")
target_link_libraries(indi_nncam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${NNCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_ogmacam_* ###########
add_executable(indi_ogmacam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_ogmacam_ccd PRIVATE "-DBUILD_OGMACAM")
target_link_libraries(indi_ogmacam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OGMACAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_ogmacam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_ogmacam_wheel PRIVATE "-DBUILD_OGMACAM")
target_link_libraries(indi_ogmacam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OGMACAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_omegonprocam_* ###########
add_executable(indi_omegonprocam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_omegonprocam_ccd PRIVATE "-DBUILD_OMEGONPROCAM")
target_link_libraries(indi_omegonprocam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OMEGONPROCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_omegonprocam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_omegonprocam_wheel PRIVATE "-DBUILD_OMEGONPROCAM")
target_link_libraries(indi_omegonprocam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${OMEGONPROCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_starshootg_* ###########
add_executable(indi_starshootg_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_starshootg_ccd PRIVATE "-DBUILD_STARSHOOTG")
target_link_libraries(indi_starshootg_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${STARSHOOTG_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_starshootg_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_starshootg_wheel PRIVATE "-DBUILD_STARSHOOTG")
target_link_libraries(indi_starshootg_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${STARSHOOTG_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_tscam_* ###########
add_executable(indi_tscam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_tscam_ccd PRIVATE "-DBUILD_TSCAM")
target_link_libraries(indi_tscam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TSCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_executable(indi_tscam_wheel ${indi_wheel_SRCS})
target_compile_definitions(indi_tscam_wheel PRIVATE "-DBUILD_TSCAM")
target_link_libraries(indi_tscam_wheel ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TSCAM_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_toupcam_ccd rt)
target_link_libraries(indi_altair_ccd rt)
target_link_libraries(indi_bressercam_ccd rt)
target_link_libraries(indi_mallincam_ccd rt)
target_link_libraries(indi_nncam_ccd rt)
target_link_libraries(indi_ogmacam_ccd rt)
target_link_libraries(indi_omegonprocam_ccd rt)
target_link_libraries(indi_starshootg_ccd rt)
target_link_libraries(indi_tscam_ccd rt)
target_link_libraries(indi_toupcam_wheel rt)
target_link_libraries(indi_altair_wheel rt)
target_link_libraries(indi_bressercam_wheel rt)
target_link_libraries(indi_mallincam_wheel rt)
target_link_libraries(indi_nncam_wheel rt)
target_link_libraries(indi_ogmacam_wheel rt)
target_link_libraries(indi_omegonprocam_wheel rt)
target_link_libraries(indi_starshootg_wheel rt)
target_link_libraries(indi_tscam_wheel rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

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
    indi_toupcam_wheel
    indi_altair_wheel
    indi_bressercam_wheel
    indi_mallincam_wheel
    indi_nncam_wheel
    indi_ogmacam_wheel
    indi_omegonprocam_wheel
    indi_starshootg_wheel
    indi_tscam_wheel
    RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml DESTINATION ${INDI_DATA_DIR})
