# - Try to find ZMQ
# Once done this will define
#
#  ZMQ_FOUND - system has ZMQ
#  ZMQ_INCLUDE_DIR - the ZMQ include directory
#  ZMQ_LIBRARIES - Link these to use ZMQ

if (ZMQ_INCLUDE_DIR AND ZMQ_LIBRARIES)

  # in cache already
  set(ZMQ_FOUND TRUE)
  message(STATUS "Found libzmq: ${ZMQ_LIBRARIES}")

else (ZMQ_INCLUDE_DIR AND ZMQ_LIBRARIES)

  find_path(ZMQ_INCLUDE_DIR
    NAMES zmq.h
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(ZMQ_LIBRARIES
    NAMES zmq
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )


  if(ZMQ_INCLUDE_DIR AND ZMQ_LIBRARIES)
    set(ZMQ_FOUND TRUE)
  else(ZMQ_INCLUDE_DIR AND ZMQ_LIBRARIES)
    set(ZMQ_FOUND FALSE)
  endif(ZMQ_INCLUDE_DIR AND ZMQ_LIBRARIES)


  if (ZMQ_FOUND)
    if (NOT ZMQ_FIND_QUIETLY)
      message(STATUS "Found ZMQ ${ZMQ_LIBRARIES}")
    endif (NOT ZMQ_FIND_QUIETLY)
  else (ZMQ_FOUND)
    if (ZMQ_FIND_REQUIRED)
      message(STATUS "ZMQ not found. Please install libzmq development package.")
    endif (ZMQ_FIND_REQUIRED)
  endif (ZMQ_FOUND)

  mark_as_advanced(ZMQ_INCLUDE_DIR ZMQ_LIBRARIES)

endif (ZMQ_INCLUDE_DIR AND ZMQ_LIBRARIES)
