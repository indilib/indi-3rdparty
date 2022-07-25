# - Try to find nutclient library (version 2) and include files
# Once done this will define
#
#  NUTCLIENT_FOUND - system has NUTCLIENT
#  NUTCLIENT_INCLUDE_DIR - the NUTCLIENT include directory
#  NUTCLIENT_LIBRARIES - Link these to use NUTCLIENT

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NUTCLIENT_INCLUDE_DIR AND NUTCLIENT_LIBRARIES)

  # in cache already
  set(NUTCLIENT_FOUND TRUE)
  message(STATUS "Found libnutclient: ${NUTCLIENT_LIBRARIES}")

else (NUTCLIENT_INCLUDE_DIR AND NUTCLIENT_LIBRARIES)

  find_path(NUTCLIENT_INCLUDE_DIR nutclient.h
    PATH_SUFFIXES nutclient
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(NUTCLIENT_LIBRARIES NAMES nutclient
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(NUTCLIENT_INCLUDE_DIR AND NUTCLIENT_LIBRARIES)
    set(NUTCLIENT_FOUND TRUE)
  else (NUTCLIENT_INCLUDE_DIR AND NUTCLIENT_LIBRARIES)
    set(NUTCLIENT_FOUND FALSE)
  endif(NUTCLIENT_INCLUDE_DIR AND NUTCLIENT_LIBRARIES)


  if (NUTCLIENT_FOUND)
    if (NOT NUTCLIENT_FIND_QUIETLY)
      message(STATUS "Found NUTCLIENT: ${NUTCLIENT_LIBRARIES}")
    endif (NOT NUTCLIENT_FIND_QUIETLY)
  else (NUTCLIENT_FOUND)
    if (NUTCLIENT_FIND_REQUIRED)
      message(FATAL_ERROR "NUTCLIENT not found. Please install libnutclient development package.")
    endif (NUTCLIENT_FIND_REQUIRED)
  endif (NUTCLIENT_FOUND)

  mark_as_advanced(NUTCLIENT_INCLUDE_DIR NUTCLIENT_LIBRARIES)

endif (NUTCLIENT_INCLUDE_DIR AND NUTCLIENT_LIBRARIES)
