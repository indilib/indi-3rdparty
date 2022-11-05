# - Try to find Finger Lakes Instruments Library
# Once done this will define
#
#  FLIPRO_FOUND - system has FLI
#  FLIPRO_INCLUDE_DIR - the FLI include directory
#  FLIPRO_LIBRARIES - Link these to use FLI

# Copyright (c) 2008, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (FLIPRO_INCLUDE_DIR AND FLIPRO_LIBRARIES)

  # in cache already
  set(FLIPRO_FOUND TRUE)
  message(STATUS "Found libflipro: ${FLIPRO_LIBRARIES}")

else (FLIPRO_INCLUDE_DIR AND FLIPRO_LIBRARIES)

  # Might need to remove lib prefix once FLI fixes their library name
  find_path(FLIPRO_INCLUDE_DIR libflipro.h
    PATH_SUFFIXES fli
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(FLIPRO_LIBRARIES NAMES libflipro
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(FLIPRO_INCLUDE_DIR AND FLIPRO_LIBRARIES)
    set(FLIPRO_FOUND TRUE)
  else (FLIPRO_INCLUDE_DIR AND FLIPRO_LIBRARIES)
    set(FLIPRO_FOUND FALSE)
  endif(FLIPRO_INCLUDE_DIR AND FLIPRO_LIBRARIES)


  if (FLIPRO_FOUND)
    if (NOT FLIPRO_FIND_QUIETLY)
      message(STATUS "Found FLIPRO: ${FLIPRO_LIBRARIES}")
    endif (NOT FLIPRO_FIND_QUIETLY)
  else (FLIPRO_FOUND)
    if (FLIPRO_FIND_REQUIRED)
      message(FATAL_ERROR "FLIPRO not found. Please install it and try again. http://www.indilib.org")
    endif (FLIPRO_FIND_REQUIRED)
  endif (FLIPRO_FOUND)

  mark_as_advanced(FLIPRO_INCLUDE_DIR FLIPRO_LIBRARIES)
  
endif (FLIPRO_INCLUDE_DIR AND FLIPRO_LIBRARIES)
