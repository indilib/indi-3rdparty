# - Try to find Moravian Instruments Camera Library
# Once done this will define
#
#  MICAM_FOUND - system has MI
#  MICAM_INCLUDE_DIR - the MI include directory
#  MICAM_LIBRARIES - Link these to use MI

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (MICAM_INCLUDE_DIR AND MICAM_LIBRARIES)

  # in cache already
  set(MICAM_FOUND TRUE)
  message(STATUS "Found libmicam: ${MICAM_LIBRARIES}")

else (MICAM_INCLUDE_DIR AND MICAM_LIBRARIES)

  find_path(MICAM_INCLUDE_DIR gxccd.h
    PATH_SUFFIXES libmicam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(MICAM_LIBRARIES NAMES gxccd
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(MICAM_INCLUDE_DIR AND MICAM_LIBRARIES)
    set(MICAM_FOUND TRUE)
  else (MICAM_INCLUDE_DIR AND MICAM_LIBRARIES)
    set(MICAM_FOUND FALSE)
  endif(MICAM_INCLUDE_DIR AND MICAM_LIBRARIES)

  if (MICAM_FOUND)
    if (NOT MICAM_FIND_QUIETLY)
      message(STATUS "Found MI Library: ${MICAM_LIBRARIES}")
    endif (NOT MICAM_FIND_QUIETLY)
  else (MICAM_FOUND)
    if (MICAM_FIND_REQUIRED)
      message(FATAL_ERROR "MI Library not found. Please install MI Library http://www.indilib.org")
    endif (MICAM_FIND_REQUIRED)
  endif (MICAM_FOUND)

  mark_as_advanced(MICAM_INCLUDE_DIR MICAM_LIBRARIES)

endif (MICAM_INCLUDE_DIR AND MICAM_LIBRARIES)
