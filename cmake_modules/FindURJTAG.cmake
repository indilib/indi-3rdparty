# - Try to find URJTAG Universal Library
# Once done this will define
#
#  URJTAG_FOUND - system has URJTAG
#  URJTAG_INCLUDE_DIR - the URJTAG include directory
#  URJTAG_LIBRARIES - Link these to use URJTAG

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (URJTAG_INCLUDE_DIR AND URJTAG_LIBRARIES)

      # in cache already
      set(URJTAG_FOUND TRUE)
      message(STATUS "Found liburjtag: ${URJTAG_LIBRARIES}")

else (URJTAG_INCLUDE_DIR AND URJTAG_LIBRARIES)

      find_path(URJTAG_INCLUDE_DIR urjtag.h
        PATH_SUFFIXES urjtag
        ${_obIncDir}
        ${GNUWIN32_DIR}/include
      )

      find_library(URJTAG_LIBRARIES NAMES urjtag
        PATHS
        ${_obLinkDir}
        ${GNUWIN32_DIR}/lib
      )

      if(URJTAG_INCLUDE_DIR AND URJTAG_LIBRARIES)
        set(URJTAG_FOUND TRUE)
      else (URJTAG_INCLUDE_DIR AND URJTAG_LIBRARIES)
        set(URJTAG_FOUND FALSE)
      endif(URJTAG_INCLUDE_DIR AND URJTAG_LIBRARIES)


      if (URJTAG_FOUND)
        if (NOT URJTAG_FIND_QUIETLY)
          message(STATUS "Found URJTAG: ${URJTAG_LIBRARIES}")
        endif (NOT URJTAG_FIND_QUIETLY)
      else (URJTAG_FOUND)
        if (URJTAG_FIND_REQUIRED)
          message(FATAL_ERROR "URJTAG not found. Please install liburjtag-dev")
        endif (URJTAG_FIND_REQUIRED)
      endif (URJTAG_FOUND)

      mark_as_advanced(URJTAG_INCLUDE_DIR URJTAG_LIBRARIES)

endif (URJTAG_INCLUDE_DIR AND URJTAG_LIBRARIES)
