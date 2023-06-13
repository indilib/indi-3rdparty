# - Try to find DFU
# Once done this will define
#
#  DFU_FOUND - system has DFU
#  DFU_INCLUDE_DIR - the DFU include directory
#  DFU_LIBRARIES - Link these to use DFU
#  DFU_VERSION_STRING - Human readable version number of dfu
#  DFU_VERSION_MAJOR  - Major version number of dfu
#  DFU_VERSION_MINOR  - Minor version number of dfu

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (DFU_INCLUDE_DIR AND DFU_LIBRARIES)

  # in cache already
  set(DFU_FOUND TRUE)
  message(STATUS "Found DFU: ${DFU_LIBRARIES}")


else (DFU_INCLUDE_DIR AND DFU_LIBRARIES)

  # JM: Packages from different distributions have different suffixes
  find_path(DFU_INCLUDE_DIR dfu.h
    PATH_SUFFIXES dfu
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(DFU_LIBRARIES NAMES dfu
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
  )

  if(DFU_INCLUDE_DIR AND DFU_LIBRARIES)
    set(DFU_FOUND TRUE)
  else (DFU_INCLUDE_DIR AND DFU_LIBRARIES)
    set(DFU_FOUND FALSE)
  endif(DFU_INCLUDE_DIR AND DFU_LIBRARIES)


  if (DFU_FOUND)

    if (NOT DFU_FIND_QUIETLY)
      message(STATUS "Found DFU ${DFU_VERSION_STRING}: ${DFU_LIBRARIES}")
    endif (NOT DFU_FIND_QUIETLY)
  else (DFU_FOUND)
    if (DFU_FIND_REQUIRED)
      message(STATUS "DFU not found.")
    endif (DFU_FIND_REQUIRED)
  endif (DFU_FOUND)

  mark_as_advanced(DFU_INCLUDE_DIR DFU_LIBRARIES)

endif (DFU_INCLUDE_DIR AND DFU_LIBRARIES)
