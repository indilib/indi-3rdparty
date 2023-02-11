# - Try to find Ogma Camera Library
# Once done this will define
#
#  OGMACAM_FOUND - system has OGMAVision
#  OGMACAM_INCLUDE_DIR - the OGMAVision include directory
#  OGMACAM_LIBRARIES - Link these to use OGMAVision

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (OGMACAM_INCLUDE_DIR AND OGMACAM_LIBRARIES)

  # in cache already
  set(OGMACAM_FOUND TRUE)
  message(STATUS "Found libogmacam: ${OGMACAM_LIBRARIES}")

else (OGMACAM_INCLUDE_DIR AND OGMACAM_LIBRARIES)

  find_path(OGMACAM_INCLUDE_DIR ogmacam.h
    PATH_SUFFIXES libogmacam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(OGMACAM_LIBRARIES NAMES ogmacam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(OGMACAM_INCLUDE_DIR AND OGMACAM_LIBRARIES)
    set(OGMACAM_FOUND TRUE)
  else (OGMACAM_INCLUDE_DIR AND OGMACAM_LIBRARIES)
    set(OGMACAM_FOUND FALSE)
  endif(OGMACAM_INCLUDE_DIR AND OGMACAM_LIBRARIES)

  if (OGMACAM_FOUND)
    if (NOT OGMACAM_FIND_QUIETLY)
      message(STATUS "Found Ogmacam: ${OGMACAM_LIBRARIES}")
    endif (NOT OGMACAM_FIND_QUIETLY)
  else (OGMACAM_FOUND)
    if (OGMACAM_FIND_REQUIRED)
      message(FATAL_ERROR "Ogmacam not found. Please install Ogmacam Library http://www.indilib.org")
    endif (OGMACAM_FIND_REQUIRED)
  endif (OGMACAM_FOUND)

  mark_as_advanced(OGMACAM_INCLUDE_DIR OGMACAM_LIBRARIES)
  
endif (OGMACAM_INCLUDE_DIR AND OGMACAM_LIBRARIES)
