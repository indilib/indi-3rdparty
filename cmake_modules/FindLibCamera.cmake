# - Try to find LIBCAMERA Library
# Once done this will define
#
#  LibCamera_FOUND - system has LibCamera
#  LibCamera_LIBRARY - Link these to use LibCamera
#  LibCamera_INCLUDE_DIR - Libcamera include Dir

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (LibCamera_INCLUDE_DIR AND LibCamera_LIBRARY)

  # in cache already
  set(LibCamera_FOUND TRUE)
  message(STATUS "Found LIBCAMERAAPPS: ${LibCamera_INCLUDE_DIR}")

else (LibCamera_INCLUDE_DIR AND LibCamera_LIBRARY)

    find_library(LibCamera_LIBRARY NAMES camera
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )    

    find_path(LibCamera_INCLUDE_DIR libcamera/camera_manager.h
    PATH_SUFFIXES libcamera
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )  

  if (LibCamera_INCLUDE_DIR AND LibCamera_LIBRARY)
    set(LibCamera_FOUND TRUE)
  else(LibCamera_INCLUDE_DIR AND LibCamera_LIBRARY)
    set(LibCamera_FOUND FALSE)
  endif(LibCamera_INCLUDE_DIR AND LibCamera_LIBRARY)

  if (LibCamera_FOUND)
    if (NOT LibCamera_FIND_QUIETLY)
      message(STATUS "Found LIBCAMERA Library: ${LibCamera_LIBRARY}")
    endif (NOT LibCamera_FIND_QUIETLY)
  else (LibCamera_FOUND)
    if (LibCamera_FIND_REQUIRED)
      message(FATAL_ERROR "LIBCAMERA Library not found. Please install libcamera-dev")
    endif (LibCamera_FIND_REQUIRED)
  endif (LibCamera_FOUND)

  mark_as_advanced(LibCamera_LIBRARY LibCamera_INCLUDE_DIR)
  
endif (LibCamera_INCLUDE_DIR AND LibCamera_LIBRARY)
