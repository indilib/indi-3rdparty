# - Try to find Astroasis Library
# Once done this will define
#
#  ASTROASIS_FOUND - system has Astroasis
#  ASTROASIS_INCLUDE_DIR - the Astroasis include directory
#  ASTROASIS_LIBRARIES - Link these to use Astroasis

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (ASTROASIS_INCLUDE_DIR AND ASTROASIS_LIBRARIES)

  # in cache already
  set(ASTROASIS_FOUND TRUE)
  message(STATUS "Found libastroasis: ${ASTROASIS_LIBRARIES}")

else (ASTROASIS_INCLUDE_DIR AND ASTROASIS_LIBRARIES)

  find_path(ASTROASIS_INCLUDE_DIR AOFocus.h
    PATH_SUFFIXES libastroasis
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(ASTROASIS_LIBRARIES NAMES oasisfocuser
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(ASTROASIS_INCLUDE_DIR AND ASTROASIS_LIBRARIES)
    set(ASTROASIS_FOUND TRUE)
  else (ASTROASIS_INCLUDE_DIR AND ASTROASIS_LIBRARIES)
    set(ASTROASIS_FOUND FALSE)
  endif(ASTROASIS_INCLUDE_DIR AND ASTROASIS_LIBRARIES)

  if (ASTROASIS_FOUND)
    if (NOT ASTROASIS_FIND_QUIETLY)
      message(STATUS "Found Astroasis Library: ${ASTROASIS_LIBRARIES}")
    endif (NOT ASTROASIS_FIND_QUIETLY)
  else (ASTROASIS_FOUND)
    if (ASTROASIS_FIND_REQUIRED)
      message(FATAL_ERROR "Astroasis Library not found. Please install Astroasis Library http://www.indilib.org")
    endif (ASTROASIS_FIND_REQUIRED)
  endif (ASTROASIS_FOUND)

  mark_as_advanced(ASTROASIS_INCLUDE_DIR ASTROASIS_LIBRARIES)
  
endif (ASTROASIS_INCLUDE_DIR AND ASTROASIS_LIBRARIES)
