# - Try to find LIBCAMERAAPPS Library
# Once done this will define
#
#  LibCameraApps_FOUND - system has LIBCAMERAAPPS
#  LibCameraApps_LIBRARY - Link these to use LibCameraApps
#  LibCameraApps_INCLUDE_DIR - LibcameraApps include Dir

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (LibCameraApps_INCLUDE_DIR AND LibCameraApps_LIBRARY)

  # in cache already
  set(LibCameraApps_FOUND TRUE)
  message(STATUS "Found LIBCAMERAAPPS: ${LibCameraApps_INCLUDE_DIR}")

else (LibCameraApps_INCLUDE_DIR AND LibCameraApps_LIBRARY)

    find_library(LibCameraApps_LIBRARY NAMES camera_app
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )    

    find_path(LibCameraApps_INCLUDE_DIR core/rpicam_app.hpp
    PATH_SUFFIXES rpicam-apps
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )  

  if (LibCameraApps_INCLUDE_DIR AND LibCameraApps_LIBRARY)
    set(LibCameraApps_FOUND TRUE)
  else(LibCameraApps_INCLUDE_DIR AND LibCameraApps_LIBRARY)
    set(LibCameraApps_FOUND FALSE)
  endif(LibCameraApps_INCLUDE_DIR AND LibCameraApps_LIBRARY)

  if (LibCameraApps_FOUND)
    if (NOT LibCameraApps_FIND_QUIETLY)
      message(STATUS "Found LIBCAMERAAPPS Library: ${LibCameraApps_LIBRARY}")
    endif (NOT LibCameraApps_FIND_QUIETLY)
  else (LibCameraApps_FOUND)
    if (LibCameraApps_FIND_REQUIRED)
      message(FATAL_ERROR "LIBCAMERAAPPS Library not found. Please install rpicam-apps")
    endif (LibCameraApps_FIND_REQUIRED)
  endif (LibCameraApps_FOUND)

  mark_as_advanced(LibCameraApps_LIBRARY LibCameraApps_INCLUDE_DIR)
  
endif (LibCameraApps_INCLUDE_DIR AND LibCameraApps_LIBRARY)
