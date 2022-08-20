# - Try to find LIBCAMERAAPPS Library
# Once done this will define
#
#  LIBCAMERAAPPS_FOUND - system has LIBCAMERAAPPS
#  LIBCAMERAAPPS_APPS - Link these to use Apps
#  LIBCAMERAAPPS_ENCODERS - Link these to use Encoders
#  LIBCAMERAAPPS_IMAGES - Link these to use Images
#  LIBCAMERAAPPS_OUTPUTS - Link these to use Outputs
#  LIBCAMERAAPPS_PREVIEW - Link these to use Preview
#  LIBCAMERAAPPS_POST - Link these to use Post Processing Stages

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (LIBCAMERAAPPS_APPS)

  # in cache already
  set(LIBCAMERAAPPS_FOUND TRUE)
  message(STATUS "Found LIBCAMERAAPPS: ${LIBCAMERAAPPS_APPS}")

else (LIBCAMERAAPPS_APPS)

    find_library(LIBCAMERAAPPS_APPS NAMES camera_app
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )

    find_library(LIBCAMERAAPPS_ENCODERS NAMES encoders
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )

    find_library(LIBCAMERAAPPS_IMAGES NAMES images
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )

    find_library(LIBCAMERAAPPS_OUTPUTS NAMES outputs
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )

    find_library(LIBCAMERAAPPS_POST NAMES post_processing_stages
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )

    find_library(LIBCAMERAAPPS_PREVIEW NAMES preview
      PATHS
      ${_obLinkDir}
      ${GNUWIN32_DIR}/lib
    )

  if(LIBCAMERAAPPS_APPS)
    set(LIBCAMERAAPPS_FOUND TRUE)
  else (LIBCAMERAAPPS_APPS)
    set(LIBCAMERAAPPS_FOUND FALSE)
  endif(LIBCAMERAAPPS_APPS)

  if (LIBCAMERAAPPS_FOUND)
    if (NOT LIBCAMERAAPPS_FIND_QUIETLY)
      message(STATUS "Found LIBCAMERAAPPS Library: ${LIBCAMERAAPPS_APPS}")
    endif (NOT LIBCAMERAAPPS_FIND_QUIETLY)
  else (LIBCAMERAAPPS_FOUND)
    if (LIBCAMERAAPPS_FIND_REQUIRED)
      message(FATAL_ERROR "LIBCAMERAAPPS Library not found. Please install libcamera-apps")
    endif (LIBCAMERAAPPS_FIND_REQUIRED)
  endif (LIBCAMERAAPPS_FOUND)

  mark_as_advanced(LIBCAMERAAPPS_APPS LIBCAMERAAPPS_ENCODERS LIBCAMERAAPPS_IMAGES LIBCAMERAAPPS_OUTPUTS LIBCAMERAAPPS_POST LIBCAMERAAPPS_PREVIEW)
  
endif (LIBCAMERAAPPS_APPS)
