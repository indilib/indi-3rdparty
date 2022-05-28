# - Try to find Omegon Pro Cam Camera Library
# Once done this will define
#
#  OMEGONPROCAM_FOUND - system has Omegon Pro Cam
#  OMEGONPROCAM_INCLUDE_DIR - the Omegon Pro Cam include directory
#  OMEGONPROCAM_LIBRARIES - Link these to use Omegon Pro Cam

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (OMEGONPROCAM_INCLUDE_DIR AND OMEGONPROCAM_LIBRARIES)

  # in cache already
  set(OMEGONPROCAM_FOUND TRUE)
  message(STATUS "Found libomegonprocam: ${OMEGONPROCAM_LIBRARIES}")

else (OMEGONPROCAM_INCLUDE_DIR AND OMEGONPROCAM_LIBRARIES)

  find_path(OMEGONPROCAM_INCLUDE_DIR omegonprocam.h
    PATH_SUFFIXES libomegonprocam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(OMEGONPROCAM_LIBRARIES NAMES omegonprocam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(OMEGONPROCAM_INCLUDE_DIR AND OMEGONPROCAM_LIBRARIES)
    set(OMEGONPROCAM_FOUND TRUE)
  else (OMEGONPROCAM_INCLUDE_DIR AND OMEGONPROCAM_LIBRARIES)
    set(OMEGONPROCAM_FOUND FALSE)
  endif(OMEGONPROCAM_INCLUDE_DIR AND OMEGONPROCAM_LIBRARIES)


  if (OMEGONPROCAM_FOUND)
    if (NOT OMEGONPROCAM_FIND_QUIETLY)
      message(STATUS "Found OmegonProCam: ${OMEGONPROCAM_LIBRARIES}")
    endif (NOT OMEGONPROCAM_FIND_QUIETLY)
  else (OMEGONPROCAM_FOUND)
    if (OMEGONPROCAM_FIND_REQUIRED)
      message(FATAL_ERROR "OmegonProCam not found. Please install OmegonProCam Library http://www.indilib.org")
    endif (OMEGONPROCAM_FIND_REQUIRED)
  endif (OMEGONPROCAM_FOUND)

  mark_as_advanced(OMEGONPROCAM_INCLUDE_DIR OMEGONPROCAM_LIBRARIES)
  
endif (OMEGONPROCAM_INCLUDE_DIR AND OMEGONPROCAM_LIBRARIES)
