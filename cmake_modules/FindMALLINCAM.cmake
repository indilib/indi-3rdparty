# - Try to find MALLINCAM Camera Library
# Once done this will define
#
#  MALLINCAM_FOUND - system has Levenhuk
#  MALLINCAM_INCLUDE_DIR - the Levenhuk include directory
#  MALLINCAM_LIBRARIES - Link these to use Levenhuk

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (MALLINCAM_INCLUDE_DIR AND MALLINCAM_LIBRARIES)

  # in cache already
  set(MALLINCAM_FOUND TRUE)
  message(STATUS "Found libnncam: ${MALLINCAM_LIBRARIES}")

else (MALLINCAM_INCLUDE_DIR AND MALLINCAM_LIBRARIES)

  find_path(MALLINCAM_INCLUDE_DIR mallincam.h
    PATH_SUFFIXES libmallincam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(MALLINCAM_LIBRARIES NAMES mallincam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(MALLINCAM_INCLUDE_DIR AND MALLINCAM_LIBRARIES)
    set(MALLINCAM_FOUND TRUE)
  else (MALLINCAM_INCLUDE_DIR AND MALLINCAM_LIBRARIES)
    set(MALLINCAM_FOUND FALSE)
  endif(MALLINCAM_INCLUDE_DIR AND MALLINCAM_LIBRARIES)

  if (MALLINCAM_FOUND)
    if (NOT MALLINCAM_FIND_QUIETLY)
      message(STATUS "Found MALLINCAM: ${MALLINCAM_LIBRARIES}")
    endif (NOT MALLINCAM_FIND_QUIETLY)
  else (MALLINCAM_FOUND)
    if (MALLINCAM_FIND_REQUIRED)
      message(FATAL_ERROR "MALLINCAM not found. Please install MALLINCAM Library http://www.indilib.org")
    endif (MALLINCAM_FIND_REQUIRED)
  endif (MALLINCAM_FOUND)

  mark_as_advanced(MALLINCAM_INCLUDE_DIR MALLINCAM_LIBRARIES)

endif (MALLINCAM_INCLUDE_DIR AND MALLINCAM_LIBRARIES)
