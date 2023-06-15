PROJECT(indi-eqmod C CXX)
cmake_minimum_required(VERSION 3.0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GSL REQUIRED)

set(EQMOD_VERSION_MAJOR 1)
set(EQMOD_VERSION_MINOR 2)

if (CYGWIN)
add_definitions(-U__STRICT_ANSI__)
endif(CYGWIN)

option(WITH_ALIGN "Enable Alignment Subsystem" ON)
option(WITH_ALIGN_GEEHALEL "Enable EQMod Alignment" ON)
option(WITH_SCOPE_LIMITS "Enable Scope limits" ON)
option(WITH_AHP_GT "Install AHP GT Controllers Driver" ON)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(WITH_AHP_GT OFF)
ENDIF()

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_eqmod.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_eqmod.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR}/..)
include_directories( ${GSL_INCLUDE_DIR} )

include(CMakeCommon)

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.9.9)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-overflow")
ENDIF ()

########### EQMod ###############
set(eqmod_CXX_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmod.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmodbase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(eqmod_CXX_SRCS ${eqmod_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
endif()

set(eqmod_CXX_SRCS ${eqmod_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
if(WITH_ALIGN_GEEHALEL)
  set(eqmod_CXX_SRCS ${eqmod_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/align/align.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/pointset.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate_chull.cpp)
  set(eqmod_C_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/align/htm.c ${CMAKE_CURRENT_SOURCE_DIR}/align/chull/chull.c)
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  set(eqmod_CXX_SRCS ${eqmod_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/scope-limits/scope-limits.cpp)
endif(WITH_SCOPE_LIMITS)

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(eqmod_c eqmod_C_SRCS 10 c)
    ENABLE_UNITY_BUILD(eqmod_cxx eqmod_CXX_SRCS 10 cpp)
ENDIF ()

add_executable(indi_eqmod_telescope ${eqmod_C_SRCS} ${eqmod_CXX_SRCS})

if(WITH_ALIGN)
  target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES} ${GSL_LIBRARIES} ${ZLIB_LIBRARY})
else(WITH_ALIGN)
  target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
endif(WITH_ALIGN)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_eqmod_telescope rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_eqmod_telescope RUNTIME DESTINATION bin )

install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_eqmod.xml indi_eqmod_sk.xml DESTINATION ${INDI_DATA_DIR})

install( FILES  simulator/indi_eqmod_simulator_sk.xml DESTINATION ${INDI_DATA_DIR})
if(WITH_ALIGN_GEEHALEL)
  install( FILES  align/indi_align_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  install( FILES  scope-limits/indi_eqmod_scope_limits_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_SCOPE_LIMITS)

########### AHP GT controllers ###############
if(WITH_AHP_GT)
    find_package(AHPGT REQUIRED)
    if(AHP_GT_FOUND)
        include_directories( ${AHP_GT_INCLUDE_DIR})
        set(ahp_gt_CXX_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/ahp-gt/ahpgt.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ahp-gt/ahpgtbase.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/eqmodbase.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

        if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
          set(ahp_gt_CXX_SRCS ${ahp_gt_CXX_SRCS}
            ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
        endif()

        set(ahp_gt_CXX_SRCS ${ahp_gt_CXX_SRCS}
           ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
        if(WITH_ALIGN_GEEHALEL)
          set(ahp_gt_CXX_SRCS ${ahp_gt_CXX_SRCS}
           ${CMAKE_CURRENT_SOURCE_DIR}/align/align.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/pointset.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate_chull.cpp)
          set(ahp_gt_C_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/align/htm.c ${CMAKE_CURRENT_SOURCE_DIR}/align/chull/chull.c)
        endif(WITH_ALIGN_GEEHALEL)
        if(WITH_SCOPE_LIMITS)
          set(ahp_gt_CXX_SRCS ${ahp_gt_CXX_SRCS}
            ${CMAKE_CURRENT_SOURCE_DIR}/scope-limits/scope-limits.cpp)
        endif(WITH_SCOPE_LIMITS)

        IF (UNITY_BUILD)
            ENABLE_UNITY_BUILD(ahp_gt_c ahp_gt_C_SRCS 10 c)
            ENABLE_UNITY_BUILD(ahp_gt_cxx ahp_gt_CXX_SRCS 10 cpp)
        ENDIF ()

        add_executable(indi_ahpgt_telescope ${ahp_gt_C_SRCS} ${ahp_gt_CXX_SRCS})

        if(WITH_ALIGN)
          target_link_libraries(indi_ahpgt_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES} ${GSL_LIBRARIES} ${ZLIB_LIBRARY})
        else(WITH_ALIGN)
          target_link_libraries(indi_ahpgt_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
        endif(WITH_ALIGN)

        target_link_libraries(indi_ahpgt_telescope ${AHP_GT_LIBRARIES})

        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
        target_link_libraries(indi_ahpgt_telescope rt)
        endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

        install(TARGETS indi_ahpgt_telescope RUNTIME DESTINATION bin )
    endif(AHP_GT_FOUND)
endif(WITH_AHP_GT)

########### AZ GTi ###############
set(azgti_CXX_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/azgti.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/azgtibase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmodbase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(azgti_CXX_SRCS ${azgti_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
endif()

set(azgti_CXX_SRCS ${azgti_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
if(WITH_ALIGN_GEEHALEL)
  set(azgti_CXX_SRCS ${azgti_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/align/align.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/pointset.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate_chull.cpp)
  set(azgti_C_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/align/htm.c ${CMAKE_CURRENT_SOURCE_DIR}/align/chull/chull.c)
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  set(azgti_CXX_SRCS ${azgti_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/scope-limits/scope-limits.cpp)
endif(WITH_SCOPE_LIMITS)

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(azgti_c azgti_C_SRCS 10 c)
    ENABLE_UNITY_BUILD(azgti_cxx azgti_CXX_SRCS 10 cpp)
ENDIF ()

add_executable(indi_azgti_telescope ${azgti_C_SRCS} ${azgti_CXX_SRCS})

if(WITH_ALIGN)
  target_link_libraries(indi_azgti_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES} ${GSL_LIBRARIES} ${ZLIB_LIBRARY})
else(WITH_ALIGN)
  target_link_libraries(indi_azgti_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
endif(WITH_ALIGN)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_azgti_telescope rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_azgti_telescope RUNTIME DESTINATION bin )

########### Sky Adventurer GTi ###############
set(skyadventurergti_CXX_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/skyadventurergti.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/skyadventurergtibase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmodbase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(skyadventurergti_CXX_SRCS ${skyadventurergti_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
endif()

set(skyadventurergti_CXX_SRCS ${skyadventurergti_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
if(WITH_ALIGN_GEEHALEL)
  set(skyadventurergti_CXX_SRCS ${skyadventurergti_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/align/align.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/pointset.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate_chull.cpp)
  set(skyadventurergti_C_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/align/htm.c ${CMAKE_CURRENT_SOURCE_DIR}/align/chull/chull.c)
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  set(skyadventurergti_CXX_SRCS ${skyadventurergti_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/scope-limits/scope-limits.cpp)
endif(WITH_SCOPE_LIMITS)

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(skyadventurergti_c skyadventurergti_C_SRCS 10 c)
    ENABLE_UNITY_BUILD(skyadventurergti_cxx skyadventurergti_CXX_SRCS 10 cpp)
ENDIF ()

add_executable(indi_skyadventurergti_telescope ${skyadventurergti_C_SRCS} ${skyadventurergti_CXX_SRCS})

if(WITH_ALIGN)
  target_link_libraries(indi_skyadventurergti_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES} ${GSL_LIBRARIES} ${ZLIB_LIBRARY})
else(WITH_ALIGN)
  target_link_libraries(indi_skyadventurergti_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
endif(WITH_ALIGN)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_skyadventurergti_telescope rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_skyadventurergti_telescope RUNTIME DESTINATION bin )

########### Star Adventurer 2i ###############
set(staradventurer2i_CXX_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/staradventurer2i.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/staradventurer2ibase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmodbase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(staradventurer2i_CXX_SRCS ${staradventurer2i_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
endif()

set(staradventurer2i_CXX_SRCS ${staradventurer2i_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
if(WITH_ALIGN_GEEHALEL)
  set(staradventurer2i_CXX_SRCS ${staradventurer2i_CXX_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/align/align.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/pointset.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate_chull.cpp)
  set(staradventurer2i_C_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/align/htm.c ${CMAKE_CURRENT_SOURCE_DIR}/align/chull/chull.c)
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  set(staradventurer2i_CXX_SRCS ${staradventurer2i_CXX_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/scope-limits/scope-limits.cpp)
endif(WITH_SCOPE_LIMITS)

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(staradventurer2i_c staradventurer2i_C_SRCS 10 c)
    ENABLE_UNITY_BUILD(staradventurer2i_cxx staradventurer2i_CXX_SRCS 10 cpp)
ENDIF ()

add_executable(indi_staradventurer2i_telescope ${staradventurer2i_C_SRCS} ${staradventurer2i_CXX_SRCS})

if(WITH_ALIGN)
  target_link_libraries(indi_staradventurer2i_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES} ${GSL_LIBRARIES} ${ZLIB_LIBRARY})
else(WITH_ALIGN)
  target_link_libraries(indi_staradventurer2i_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
endif(WITH_ALIGN)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_staradventurer2i_telescope rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_staradventurer2i_telescope RUNTIME DESTINATION bin )

###################################################################################################
#########################################  Tests  #################################################
###################################################################################################

set(INDI_BUILD_UNITTESTS TRUE)

find_package (GTest)
find_package (GMock)
IF (GTEST_FOUND)
  IF (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Building unit tests")
    ADD_SUBDIRECTORY(test)
  ELSE (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Not building unit tests")
  ENDIF (INDI_BUILD_UNITTESTS)
ELSE()
  MESSAGE (STATUS  "GTEST not found, not building unit tests")
ENDIF (GTEST_FOUND)
