# - Try to find SV305 Library
# Once done this will define
#
#  SV305_FOUND - system has QHY
#  SV305_INCLUDE_DIR - the QHY include directory
#  SV305_LIBRARIES - Link these to use QHY

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (SV305_INCLUDE_DIR AND SV305_LIBRARIES)

  # in cache already
  set(SV305_FOUND TRUE)
  message(STATUS "Found libsv305: ${SV305_LIBRARIES}")

else (SV305_INCLUDE_DIR AND SV305_LIBRARIES)

  # find headers
  find_path(SV305_INCLUDE_DIR NAMES SVBCameraSDK.h
    PATH_SUFFIXES libsv305
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  # find libraries
  find_library(SV305_LIBRARIES NAMES SVBCameraSDK
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(SV305_INCLUDE_DIR AND SV305_LIBRARIES)
    set(SV305_FOUND TRUE)
  else (SV305_INCLUDE_DIR AND SV305_LIBRARIES)
    set(SV305_FOUND FALSE)
  endif(SV305_INCLUDE_DIR AND SV305_LIBRARIES)


  if (SV305_FOUND)
    if (NOT SV305_FIND_QUIETLY)
      message(STATUS "Found SV305 libraries : ${SV305_LIBRARIES}")
    endif (NOT SV305_FIND_QUIETLY)
  else (SV305_FOUND)
    if (SV305_FIND_REQUIRED)
      message(FATAL_ERROR "SV305 libraries not found. Please install libsv305 http://www.indilib.org")
    endif (SV305_FIND_REQUIRED)
  endif (SV305_FOUND)

  mark_as_advanced(SV305_INCLUDE_DIR SV305_LIBRARIES)

endif (SV305_INCLUDE_DIR AND SV305_LIBRARIES)
