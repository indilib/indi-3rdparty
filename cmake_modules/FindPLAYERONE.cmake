# - Try to find PlayerOne Library
# Once done this will define
#
#  PLAYERONE_FOUND - system has PLAYERONE
#  PLAYERONE_INCLUDE_DIR - the PLAYERONE include directory
#  PLAYERONE_LIBRARIES - Link these to use ASI

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARIES)

  # in cache already
  set(PLAYERONE_FOUND TRUE)
  message(STATUS "Found libplayerone: ${PLAYERONE_LIBRARIES}")

else (PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARIES)

  find_path(PLAYERONE_INCLUDE_DIR PlayerOneCamera.h
    PATH_SUFFIXES libplayerone
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(PLAYERONECAM_LIBRARIES NAMES PlayerOneCamera
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )


  if (PLAYERONECAM_LIBRARIES)
    set(PLAYERONE_LIBRARIES ${PLAYERONECAM_LIBRARIES})
  endif (PLAYERONECAM_LIBRARIES)


  if(PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARIES)
    set(PLAYERONE_FOUND TRUE)
  else (PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARIES)
    set(PLAYERONE_FOUND FALSE)
  endif(PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARIES)


  if (PLAYERONE_FOUND)
    if (NOT PLAYERONE_FIND_QUIETLY)
      message(STATUS "Found PLAYERONE: ${PLAYERONE_LIBRARIES}")
    endif (NOT PLAYERONE_FIND_QUIETLY)
  else (PLAYERONE_FOUND)
    if (PLAYERONE_FIND_REQUIRED)
      message(FATAL_ERROR "PLAYERONE not found. Please install libPlayerOneCamera.3 http://www.indilib.org")
    endif (PLAYERONE_FIND_REQUIRED)
  endif (PLAYERONE_FOUND)

  mark_as_advanced(PLAYERONE_INCLUDE_DIR PLAYERONE_LIBRARIES)

endif (PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARIES)
