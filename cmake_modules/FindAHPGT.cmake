# - Try to find AHPGT
# Once done this will define
#
#  AHPGT_FOUND - system has AHPGT
#  AHPGT_INCLUDE_DIR - the AHPGT include directory
#  AHPGT_LIBRARIES - Link these to use AHPGT
#  AHPGT_VERSION_STRING - Human readable version number of ahp_gt
#  AHPGT_VERSION_MAJOR  - Major version number of ahp_gt
#  AHPGT_VERSION_MINOR  - Minor version number of ahp_gt

# Copyright (c) 2017, Ilia Platone, <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (AHPGT_INCLUDE_DIR AND AHPGT_LIBRARIES)

  # in cache already
  set(AHPGT_FOUND TRUE)
  message(STATUS "Found AHPGT: ${AHPGT_LIBRARIES}")


else (AHPGT_INCLUDE_DIR AND AHPGT_LIBRARIES)

    find_path(AHPGT_INCLUDE_DIR ahp_gt.h
      PATH_SUFFIXES ahp
      ${_obIncDir}
      ${GNUWIN32_DIR}/include
    )

  find_library(AHPGT_LIBRARIES NAMES ahp_gt
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
    HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
  )

if(AHPGT_INCLUDE_DIR AND AHPGT_LIBRARIES)
  set(AHPGT_FOUND TRUE)
else (AHPGT_INCLUDE_DIR AND AHPGT_LIBRARIES)
  set(AHPGT_FOUND FALSE)
endif(AHPGT_INCLUDE_DIR AND AHPGT_LIBRARIES)

  if (AHPGT_FOUND)
    if (NOT AHPGT_FIND_QUIETLY)
      message(STATUS "Found AHPGT: ${AHPGT_LIBRARIES}")
    endif (NOT AHPGT_FIND_QUIETLY)
  else (AHPGT_FOUND)
    if (AHPGT_FIND_REQUIRED)
      message(FATAL_ERROR "AHPGT not found. Please install libahp_gt-dev")
    endif (AHPGT_FIND_REQUIRED)
  endif (AHPGT_FOUND)

  mark_as_advanced(AHPGT_LIBRARIES)
  
endif (AHPGT_INCLUDE_DIR AND AHPGT_LIBRARIES)
