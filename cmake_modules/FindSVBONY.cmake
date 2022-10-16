# - Try to find SVBONY Library
# Once done this will define
#
#  SVBONY_FOUND - system has SVBONY CCD
#  SVBONY_INCLUDE_DIR - the SVBONY CCD include directory
#  SVBONY_LIBRARIES - Link these to use SVBONY CCD

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (SVBONY_INCLUDE_DIR AND SVBONY_LIBRARIES)

  # in cache already
  set(SVBONY_FOUND TRUE)
  message(STATUS "Found libsvbony: ${SVBONY_LIBRARIES}")

else (SVBONY_INCLUDE_DIR AND SVBONY_LIBRARIES)

  # find headers
  find_path(SVBONY_INCLUDE_DIR NAMES SVBCameraSDK.h
    PATH_SUFFIXES libsvbony
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  # find libraries
  find_library(SVBONY_LIBRARIES NAMES SVBCameraSDK
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(SVBONY_INCLUDE_DIR AND SVBONY_LIBRARIES)
    set(SVBONY_FOUND TRUE)
  else (SVBONY_INCLUDE_DIR AND SVBONY_LIBRARIES)
    set(SVBONY_FOUND FALSE)
  endif(SVBONY_INCLUDE_DIR AND SVBONY_LIBRARIES)


  if (SVBONY_FOUND)
    if (NOT SVBONY_FIND_QUIETLY)
      message(STATUS "Found SVBONY libraries : ${SVBONY_LIBRARIES}")
    endif (NOT SVBONY_FIND_QUIETLY)
  else (SVBONY_FOUND)
    if (SVBONY_FIND_REQUIRED)
      message(FATAL_ERROR "SVBONY libraries not found. Please install libsvbony http://www.indilib.org")
    endif (SVBONY_FIND_REQUIRED)
  endif (SVBONY_FOUND)

  mark_as_advanced(SVBONY_INCLUDE_DIR SVBONY_LIBRARIES)

endif (SVBONY_INCLUDE_DIR AND SVBONY_LIBRARIES)
