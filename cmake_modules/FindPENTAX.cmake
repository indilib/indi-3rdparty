# - Try to find PENTAX Universal Library
# Once done this will define
#
#  PENTAX_FOUND - system has PENTAX
#  PENTAX_INCLUDE_DIR - the PENTAX include directory
#  PENTAX_LIBRARIES - Link these to use PENTAX

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)

  # in cache already
  set(PENTAX_FOUND TRUE)
  message(STATUS "Found libpentax: ${PENTAX_LIBRARIES}")

else (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)

  find_path(PENTAX_INCLUDE_DIR ricoh_camera_sdk.hpp
    PATH_SUFFIXES libpentax
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(RICOH_LIBRARIES NAMES RicohCameraSDKCpp 
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  find_library(MTP_LIBRARIES NAMES mtp 
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if (RICOH_LIBRARIES AND MTP_LIBRARIES)
    set(PENTAX_LIBRARIES ${RICOH_LIBRARIES} ${MTP_LIBRARIES})
  endif (RICOH_LIBRARIES AND MTP_LIBRARIES)

  if(PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)
    set(PENTAX_FOUND TRUE)
  else (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)
    set(PENTAX_FOUND FALSE)
  endif(PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)


  if (PENTAX_FOUND)
    if (NOT PENTAX_FIND_QUIETLY)
      message(STATUS "Found PENTAX: ${PENTAX_LIBRARIES}")
    endif (NOT PENTAX_FIND_QUIETLY)
  else (PENTAX_FOUND)
    if (PENTAX_FIND_REQUIRED)
      message(FATAL_ERROR "PENTAX not found. Please install PENTAX Library http://www.indilib.org")
    endif (PENTAX_FIND_REQUIRED)
  endif (PENTAX_FOUND)

  mark_as_advanced(PENTAX_INCLUDE_DIR PENTAX_LIBRARIES)
  
endif (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)
