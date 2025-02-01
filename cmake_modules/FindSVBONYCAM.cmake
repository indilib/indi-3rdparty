# - Try to find Teleskop Camera Library
# Once done this will define
#
#  SVBONYCAM_FOUND - system has SVBONY
#  SVBONYCAM_INCLUDE_DIR - the SVBONY include directory
#  SVBONYCAM_LIBRARIES - Link these to use Teleskop

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (SVBONYCAM_INCLUDE_DIR AND SVBONYCAM_LIBRARIES)

  # in cache already
  set(SVBONYCAM_FOUND TRUE)
  message(STATUS "Found libsvbonycam: ${SVBONYCAM_LIBRARIES}")

else (SVBONYCAM_INCLUDE_DIR AND SVBONYCAM_LIBRARIES)

  find_path(SVBONYCAM_INCLUDE_DIR svbonycam.h
    PATH_SUFFIXES libsvbonycam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(SVBONYCAM_LIBRARIES NAMES svbonycam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(SVBONYCAM_INCLUDE_DIR AND SVBONYCAM_LIBRARIES)
    set(SVBONYCAM_FOUND TRUE)
  else (SVBONYCAM_INCLUDE_DIR AND SVBONYCAM_LIBRARIES)
    set(SVBONYCAM_FOUND FALSE)
  endif(SVBONYCAM_INCLUDE_DIR AND SVBONYCAM_LIBRARIES)

  if (SVBONYCAM_FOUND)
    if (NOT SVBONYCAM_FIND_QUIETLY)
      message(STATUS "Found Svbonycam: ${SVBONYCAM_LIBRARIES}")
    endif (NOT SVBONYCAM_FIND_QUIETLY)
  else (SVBONYCAM_FOUND)
    if (SVBONYCAM_FIND_REQUIRED)
      message(FATAL_ERROR "Svbonycam not found. Please install Svbonycam Library http://www.indilib.org")
    endif (SVBONYCAM_FIND_REQUIRED)
  endif (SVBONYCAM_FOUND)

  mark_as_advanced(SVBONYCAM_INCLUDE_DIR SVBONYCAM_LIBRARIES)
  
endif (SVBONYCAM_INCLUDE_DIR AND SVBONYCAM_LIBRARIES)
