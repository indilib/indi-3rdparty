# - Try to find Bresser Camera Library
# Once done this will define
#
#  BRESSERCAM_FOUND - system has Bresser
#  BRESSERCAM_INCLUDE_DIR - the Bresser include directory
#  BRESSERCAM_LIBRARIES - Link these to use Bresser

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (BRESSERCAM_INCLUDE_DIR AND BRESSERCAM_LIBRARIES)

  # in cache already
  set(BRESSERCAM_FOUND TRUE)
  message(STATUS "Found libbressercam: ${BRESSERCAM_LIBRARIES}")

else (BRESSERCAM_INCLUDE_DIR AND BRESSERCAM_LIBRARIES)

  find_path(BRESSERCAM_INCLUDE_DIR bressercam.h
    PATH_SUFFIXES libbressercam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(BRESSERCAM_LIBRARIES NAMES bressercam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(BRESSERCAM_INCLUDE_DIR AND BRESSERCAM_LIBRARIES)
    set(BRESSERCAM_FOUND TRUE)
  else (BRESSERCAM_INCLUDE_DIR AND BRESSERCAM_LIBRARIES)
    set(BRESSERCAM_FOUND FALSE)
  endif(BRESSERCAM_INCLUDE_DIR AND BRESSERCAM_LIBRARIES)

  if (BRESSERCAM_FOUND)
    if (NOT BRESSERCAM_FIND_QUIETLY)
      message(STATUS "Found Bressercam: ${BRESSERCAM_LIBRARIES}")
    endif (NOT BRESSERCAM_FIND_QUIETLY)
  else (BRESSERCAM_FOUND)
    if (BRESSERCAM_FIND_REQUIRED)
      message(FATAL_ERROR "Bressercam not found. Please install Bressercam Library http://www.indilib.org")
    endif (BRESSERCAM_FIND_REQUIRED)
  endif (BRESSERCAM_FOUND)

  mark_as_advanced(BRESSERCAM_INCLUDE_DIR BRESSERCAM_LIBRARIES)
  
endif (BRESSERCAM_INCLUDE_DIR AND BRESSERCAM_LIBRARIES)
