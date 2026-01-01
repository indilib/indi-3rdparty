# - Try to find AHP_XC
# Once done this will define
#
#  AHP_XC_FOUND - system has AHP_XC
#  AHP_XC_INCLUDE_DIR - the AHP_XC include directory
#  AHP_XC_LIBRARIES - Link these to use AHP_XC
#  AHP_XC_VERSION_STRING - Human readable version number of ahp_gt
#  AHP_XC_VERSION_MAJOR  - Major version number of ahp_gt
#  AHP_XC_VERSION_MINOR  - Minor version number of ahp_gt

# Copyright (c) 2017, Ilia Platone, <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)

  # in cache already
  set(AHP_XC_FOUND TRUE)
  message(STATUS "Found AHP_XC: ${AHP_XC_LIBRARIES}")

else (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)

    set(AHP_XC_MIN_VERSION_MAJOR 1)
    set(AHP_XC_MIN_VERSION_MINOR 4)
    set(AHP_XC_MIN_VERSION_RELEASE 5)

    find_path(AHP_XC_INCLUDE_DIR ahp_xc.h
      PATH_SUFFIXES ahp
      ${_obIncDir}
      ${GNUWIN32_DIR}/include
    )

  find_library(AHP_XC_LIBRARIES NAMES ahp_xc
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
    HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
  )

    if(AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)
        set(AHP_XC_FOUND TRUE)
    else(AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)
        set(AHP_XC_FOUND FALSE)
    endif(AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)

  if (AHP_XC_FOUND)
    if (NOT AHP_XC_FIND_QUIETLY)
        if(AHP_XC_FOUND)
            include(CheckCXXSourceCompiles)
            include(CMakePushCheckState)
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_INCLUDES ${AHP_XC_INCLUDE_DIR})
            set(CMAKE_REQUIRED_LIBRARIES ${AHP_XC_LIBRARIES})
            check_cxx_source_compiles("#include <ahp_xc.h>
            int main() { ahp_xc_get_version(); return 0; }" AHP_XC_HAS_AHP_XC_VERSION)
            cmake_pop_check_state()
        endif()
      message(STATUS "Found AHP_XC: ${AHP_XC_LIBRARIES}")
    endif (NOT AHP_XC_FIND_QUIETLY)
  else (AHP_XC_FOUND)
    if (AHP_XC_FIND_REQUIRED)
      message(FATAL_ERROR "AHP_XC not found. Please install libahp_xc-dev")
    endif (AHP_XC_FIND_REQUIRED)
  endif (AHP_XC_FOUND)

  mark_as_advanced(AHP_XC_LIBRARIES)

endif (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)
