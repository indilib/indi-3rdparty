# - Find LIBTIC1
# Find the Pololu Tic Library

pkg_check_modules(LIBTIC1 QUIET libpololu-tic-1)

find_path(LIBTIC1_INCLUDE_DIR
  NAMES
    tic.h
  HINTS
    ${LIBTIC1_INCLUDE_DIRS}
  PATH_SUFFIXES
    libpololu-tic-1
)

find_library(LIBTIC1_LIBRARY
  NAMES
    ${LIBTIC1_LIBRARIES}
    libpololu-tic-1
  HINTS
    ${LIBTIC1_LIBRARY_DIRS}
)

set(LIBTIC1_INCLUDE_DIRS ${LIBTIC1_INCLUDE_DIR})
set(LIBTIC1_LIBRARIES ${LIBTIC1_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBTIC1
  FOUND_VAR
    LIBTIC1_FOUND
  REQUIRED_VARS
    LIBTIC1_LIBRARY
    LIBTIC1_INCLUDE_DIR
  VERSION_VAR
    PC_LIBLIBTIC1_VERSION
)

set(LIBTIC1_INCLUDE_DIRS ${LIBTIC1_INCLUDE_DIR})
set(LIBTIC1_LIBRARIES ${LIBTIC1_LIBRARY})

mark_as_advanced(LIBTIC1_INCLUDE_DIRS LIBTIC1_LIBRARIES)

IF(LIBTIC1_FOUND)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${LIBTIC1_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${LIBTIC1_LIBRARIES})
  cmake_pop_check_state()
ENDIF(LIBTIC1_FOUND)
