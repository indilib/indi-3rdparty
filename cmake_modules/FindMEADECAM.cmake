# - Try to find Meade Cam Camera Library
# Once done this will define
#
#  MEADECAM_FOUND - system has Meade Cam
#  MEADECAM_INCLUDE_DIR - the Meade Cam include directory
#  MEADECAM_LIBRARIES - Link these to use Meade Cam

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (MEADECAM_INCLUDE_DIR AND MEADECAM_LIBRARIES)

  # in cache already
  set(MEADECAM_FOUND TRUE)
  message(STATUS "Found libmeadecam: ${MEADECAM_LIBRARIES}")

else (MEADECAM_INCLUDE_DIR AND MEADECAM_LIBRARIES)

  find_path(MEADECAM_INCLUDE_DIR meadecam.h
    PATH_SUFFIXES libmeadecam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(MEADECAM_LIBRARIES NAMES meadecam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(MEADECAM_INCLUDE_DIR AND MEADECAM_LIBRARIES)
    set(MEADECAM_FOUND TRUE)
  else (MEADECAM_INCLUDE_DIR AND MEADECAM_LIBRARIES)
    set(MEADECAM_FOUND FALSE)
  endif(MEADECAM_INCLUDE_DIR AND MEADECAM_LIBRARIES)


  if (MEADECAM_FOUND)
    if (NOT MEADECAM_FIND_QUIETLY)
	    message(STATUS "Found MeadeCam: ${MEADECAM_LIBRARIES}")
    endif (NOT MEADECAM_FIND_QUIETLY)
  else (MEADECAM_FOUND)
    if (MEADECAM_FIND_REQUIRED)
	    message(FATAL_ERROR "MeadeCam not found. Please install MeadeCam Library http://www.indilib.org")
    endif (MEADECAM_FIND_REQUIRED)
  endif (MEADECAM_FOUND)

  mark_as_advanced(MEADECAM_INCLUDE_DIR MEADECAM_LIBRARIES)
  
endif (MEADECAM_INCLUDE_DIR AND MEADECAM_LIBRARIES)
