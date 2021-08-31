# - Try to find PlayerOne Library
# Once done this will define
#
#  POA_FOUND - system has POA
#  POA_INCLUDE_DIR - the POA include directory
#  POA_LIBRARIES - Link these to use ASI

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (POA_INCLUDE_DIR AND POA_LIBRARIES)

  # in cache already
  set(POA_FOUND TRUE)
  message(STATUS "Found libplayerone: ${POA_LIBRARIES}")

else (POA_INCLUDE_DIR AND POA_LIBRARIES)

  find_path(POA_INCLUDE_DIR PlayerOneCamera.h
    PATH_SUFFIXES libplayerone
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(POACAM_LIBRARIES NAMES PlayerOneCamera
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )


  if (POACAM_LIBRARIES)
    set(POA_LIBRARIES ${POACAM_LIBRARIES})
  endif (POACAM_LIBRARIES)


  if(POA_INCLUDE_DIR AND POA_LIBRARIES)
    set(POA_FOUND TRUE)
  else (POA_INCLUDE_DIR AND POA_LIBRARIES)
    set(POA_FOUND FALSE)
  endif(POA_INCLUDE_DIR AND POA_LIBRARIES)


  if (POA_FOUND)
    if (NOT POA_FIND_QUIETLY)
      message(STATUS "Found PLAYERONE: ${POA_LIBRARIES}")
    endif (NOT POA_FIND_QUIETLY)
  else (POA_FOUND)
    if (POA_FIND_REQUIRED)
      message(FATAL_ERROR "POA not found. Please install libPlayerOneCamera.2 http://www.indilib.org")
    endif (POA_FIND_REQUIRED)
  endif (POA_FOUND)

  mark_as_advanced(POA_INCLUDE_DIR POA_LIBRARIES)

endif (POA_INCLUDE_DIR AND POA_LIBRARIES)
