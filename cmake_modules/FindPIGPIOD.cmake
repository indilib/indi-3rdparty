# - Try to find PIGPIOD Library
# Once done this will define
#
#  PIGPIOD_FOUND - system has PIGPIOD
#  PIGPIOD_INCLUDE_DIR - the PIGPIOD include directory
#  PIGPIOD_LIBRARIES - Link these to use PIGPIOD

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (PIGPIOD_INCLUDE_DIR AND PIGPIOD_LIBRARIES)

  # in cache already
  set(PIGPIOD_FOUND TRUE)
  message(STATUS "Found libpigpiod: ${PIGPIOD_LIBRARIES}")

else (PIGPIOD_INCLUDE_DIR AND PIGPIOD_LIBRARIES)

  find_path(PIGPIOD_INCLUDE_DIR pigpio.h
    PATH_SUFFIXES libpigpiod
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_program(PIGPIOD_LIBRARIES NAMES pigpiod
    PATHS
    ${GNUWIN32_DIR}/bin
  )

  if(PIGPIOD_INCLUDE_DIR AND PIGPIOD_LIBRARIES)
    set(PIGPIOD_FOUND TRUE)
  else (PIGPIOD_INCLUDE_DIR AND PIGPIOD_LIBRARIES)
    set(PIGPIOD_FOUND FALSE)
  endif(PIGPIOD_INCLUDE_DIR AND PIGPIOD_LIBRARIES)


  if (PIGPIOD_FOUND)
    if (NOT PIGPIOD_FIND_QUIETLY)
      message(STATUS "Found PIGPIOD: ${PIGPIOD_LIBRARIES}")
    endif (NOT PIGPIOD_FIND_QUIETLY)
  else (PIGPIOD_FOUND)
    if (PIGPIOD_FIND_REQUIRED)
      message(FATAL_ERROR "PIGPIOD not found. Please install PIGPIOD Library http://www.indilib.org")
    endif (PIGPIOD_FIND_REQUIRED)
  endif (PIGPIOD_FOUND)

  mark_as_advanced(PIGPIOD_INCLUDE_DIR PIGPIOD_LIBRARIES)
  
endif (PIGPIOD_INCLUDE_DIR AND PIGPIOD_LIBRARIES)
