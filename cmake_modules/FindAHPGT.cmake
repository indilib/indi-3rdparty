# - Try to find AHP_GT
# Once done this will define
#
#  AHP_GT_FOUND - system has AHP_GT
#  AHP_GT_INCLUDE_DIR - the AHP_GT include directory
#  AHP_GT_LIBRARIES - Link these to use AHP_GT
#  AHP_GT_VERSION_STRING - Human readable version number of ahp_gt
#  AHP_GT_VERSION_MAJOR  - Major version number of ahp_gt
#  AHP_GT_VERSION_MINOR  - Minor version number of ahp_gt

# Copyright (c) 2017, Ilia Platone, <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (AHP_GT_INCLUDE_DIR AND AHP_GT_LIBRARIES)

  # in cache already
  set(AHP_GT_FOUND TRUE)
  message(STATUS "Found AHP_GT: ${AHP_GT_LIBRARIES}")


else (AHP_GT_INCLUDE_DIR AND AHP_GT_LIBRARIES)

    find_path(AHP_GT_INCLUDE_DIR ahp_gt.h
      PATH_SUFFIXES ahp
      ${_obIncDir}
      ${GNUWIN32_DIR}/include
    )

  find_library(AHP_GT_LIBRARIES NAMES ahp_gt
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
    HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
  )

if(AHP_GT_INCLUDE_DIR AND AHP_GT_LIBRARIES)
  set(AHP_GT_FOUND TRUE)
else (AHP_GT_INCLUDE_DIR AND AHP_GT_LIBRARIES)
  set(AHP_GT_FOUND FALSE)
endif(AHP_GT_INCLUDE_DIR AND AHP_GT_LIBRARIES)

  if (AHP_GT_FOUND)
    if (NOT AHP_GT_FIND_QUIETLY)
      message(STATUS "Found AHP_GT: ${AHP_GT_LIBRARIES}")
    endif (NOT AHP_GT_FIND_QUIETLY)
  else (AHP_GT_FOUND)
    if (AHP_GT_FIND_REQUIRED)
      message(FATAL_ERROR "AHP_GT not found. Please install libahp_gt-dev")
    endif (AHP_GT_FIND_REQUIRED)
  endif (AHP_GT_FOUND)

  mark_as_advanced(AHP_GT_LIBRARIES)
  
endif (AHP_GT_INCLUDE_DIR AND AHP_GT_LIBRARIES)
