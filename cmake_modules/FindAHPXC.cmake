# - Try to find AHP_XC Universal Library
# Once done this will define
#
#  AHP_XC_FOUND - system has AHP_XC
#  AHP_XC_INCLUDE_DIR - the AHP_XC include directory
#  AHP_XC_LIBRARIES - Link these to use AHP_XC

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)

      # in cache already
      set(AHP_XC_FOUND TRUE)
      message(STATUS "Found libahp_xc: ${AHP_XC_LIBRARIES}")

else (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)

      find_path(AHP_XC_INCLUDE_DIR ahp_xc.h
        PATH_SUFFIXES ahp
        ${_obIncDir}
        ${GNUWIN32_DIR}/include
      )

      find_library(AHP_XC_LIBRARIES NAMES ahp_xc
        PATHS
        ${_obLinkDir}
        ${GNUWIN32_DIR}/lib
      )

      if(AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)
        set(AHP_XC_FOUND TRUE)
      else (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)
        set(AHP_XC_FOUND FALSE)
      endif(AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)


      if (AHP_XC_FOUND)
        if (NOT AHP_XC_FIND_QUIETLY)
          message(STATUS "Found AHP XC: ${AHP_XC_LIBRARIES}")
        endif (NOT AHP_XC_FIND_QUIETLY)
      else (AHP_XC_FOUND)
        if (AHP_XC_FIND_REQUIRED)
          message(FATAL_ERROR "AHP XC not found. Please install libahp_xc http://www.indilib.org")
        endif (AHP_XC_FIND_REQUIRED)
      endif (AHP_XC_FOUND)

      mark_as_advanced(AHP_XC_INCLUDE_DIR AHP_XC_LIBRARIES)

endif (AHP_XC_INCLUDE_DIR AND AHP_XC_LIBRARIES)
