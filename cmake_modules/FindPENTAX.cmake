# - Try to find PENTAX Universal Libraries
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
  message(STATUS "Found PENTAX libraries: ${PENTAX_LIBRARIES}")

else (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)

  find_path(PKTRIGGERCORD_INCLUDE_DIR libpktriggercord.h
    PATH_SUFFIXES libpktriggercord
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(PKTRIGGERCORD_LIBRARIES NAMES pktriggercord 
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    PATH_SUFFIXES indipentax
  )

#if not armv8, then look for ricoh library; otherwise only use pktriggercord library
    if(NOT (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^aarch64"))
      find_path(RICOH_INCLUDE_DIR ricoh_camera_sdk.hpp
        PATH_SUFFIXES libpentax libricohcamerasdk
        ${_obIncDir}
        ${GNUWIN32_DIR}/include
      )
      find_library(RICOH_LIBRARIES NAMES RicohCameraSDKCpp 
        PATHS
        ${_obLinkDir}
        ${GNUWIN32_DIR}/lib
      )
      find_library(RICOHMTP_LIBRARIES NAMES libmtpricoh.so.9.3.0 
        PATHS
        ${_obLinkDir}
        ${GNUWIN32_DIR}/lib
      )

      if (RICOH_INCLUDE_DIR AND PKTRIGGERCORD_INCLUDE_DIR)
        set(PENTAX_INCLUDE_DIR ${RICOH_INCLUDE_DIR} ${PKTRIGGERCORD_INCLUDE_DIR})
      endif (RICOH_INCLUDE_DIR AND PKTRIGGERCORD_INCLUDE_DIR)
      if (RICOH_LIBRARIES AND RICOHMTP_LIBRARIES AND PKTRIGGERCORD_LIBRARIES)
        set(PENTAX_LIBRARIES ${RICOH_LIBRARIES} ${RICOHMTP_LIBRARIES} ${PKTRIGGERCORD_LIBRARIES})
      endif (RICOH_LIBRARIES AND RICOHMTP_LIBRARIES AND PKTRIGGERCORD_LIBRARIES)

    else()
      if (PKTRIGGERCORD_INCLUDE_DIR)
        set(PENTAX_INCLUDE_DIR ${PKTRIGGERCORD_INCLUDE_DIR})
      endif (PKTRIGGERCORD_INCLUDE_DIR)
      if (PKTRIGGERCORD_LIBRARIES)
        set(PENTAX_LIBRARIES ${PKTRIGGERCORD_LIBRARIES})
      endif (PKTRIGGERCORD_LIBRARIES)
    endif()


  if(PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)
    set(PENTAX_FOUND TRUE)
  else (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)
    set(PENTAX_FOUND FALSE)
  endif(PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)


  if (PENTAX_FOUND)
    if (NOT PENTAX_FIND_QUIETLY)
      message(STATUS "Found PENTAX libraries: ${PENTAX_LIBRARIES}")
    endif (NOT PENTAX_FIND_QUIETLY)
  else (PENTAX_FOUND)
    if (PENTAX_FIND_REQUIRED)
      message(FATAL_ERROR "One or both of libricohcamersdk and libpktriggercord are not found.  Please install them.  See http://www.indilib.org.")
    endif (PENTAX_FIND_REQUIRED)
  endif (PENTAX_FOUND)

  mark_as_advanced(PENTAX_INCLUDE_DIR PENTAX_LIBRARIES)
  
endif (PENTAX_INCLUDE_DIR AND PENTAX_LIBRARIES)
