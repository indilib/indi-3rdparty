# - Try to find Teleskop Camera Library
# Once done this will define
#
#  TSCAM_FOUND - system has Teleskop 
#  TSCAM_INCLUDE_DIR - the Teleskop include directory
#  TSCAM_LIBRARIES - Link these to use Teleskop

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (TSCAM_INCLUDE_DIR AND TSCAM_LIBRARIES)

  # in cache already
  set(TSCAM_FOUND TRUE)
  message(STATUS "Found libtscam: ${TSCAM_LIBRARIES}")

else (TSCAM_INCLUDE_DIR AND TSCAM_LIBRARIES)

  find_path(TSCAM_INCLUDE_DIR tscam.h
    PATH_SUFFIXES libtscam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(TSCAM_LIBRARIES NAMES tscam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(TSCAM_INCLUDE_DIR AND TSCAM_LIBRARIES)
    set(TSCAM_FOUND TRUE)
  else (TSCAM_INCLUDE_DIR AND TSCAM_LIBRARIES)
    set(TSCAM_FOUND FALSE)
  endif(TSCAM_INCLUDE_DIR AND TSCAM_LIBRARIES)

  if (TSCAM_FOUND)
    if (NOT TSCAM_FIND_QUIETLY)
      message(STATUS "Found Tscam: ${TSCAM_LIBRARIES}")
    endif (NOT TSCAM_FIND_QUIETLY)
  else (TSCAM_FOUND)
    if (TSCAM_FIND_REQUIRED)
      message(FATAL_ERROR "Tscam not found. Please install Tscam Library http://www.indilib.org")
    endif (TSCAM_FIND_REQUIRED)
  endif (TSCAM_FOUND)

  mark_as_advanced(TSCAM_INCLUDE_DIR TSCAM_LIBRARIES)
  
endif (TSCAM_INCLUDE_DIR AND TSCAM_LIBRARIES)
