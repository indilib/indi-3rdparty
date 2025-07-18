use_cxx11()

pkg_check_modules(LIBUSBP QUIET libusbp-1)
string (REPLACE ";" " " LIBUSBP_CFLAGS_STR "${LIBUSBP_CFLAGS}")
string (REPLACE ";" " " LIBUSBP_LDFLAGS_STR "${LIBUSBP_LDFLAGS}")

if (USE_SYSTEM_LIBTINYXML2)
  pkg_check_modules(LIBTINXML2 tinyxml2)
  If (NOT LIBTINXML2_FOUND)
  message(STATUS "LIBTINXML2 (tinyxml2) not found on system. The tinyxml2 library is required. Please install tinyxml2.")
  endif ()
  STRING(REPLACE ";" " " LIBTINYXML2_CFLAGS_STR "${LIBTINYXML2_CFLAGS}")
  STRING(REPLACE ";" " " LIBTINYXML2_LDFLAGS_STR "${LIBTINYXML2_LDFLAGS}")
else ()
  set (LIBTINYXML2_SRC libtinyxml2/tinyxml2.cpp)
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/libtinyxml2")
endif ()

set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${LIBUSBP_CFLAGS_STR} ${LIBTINYXML2_CFLAGS_STR}")

add_library (bootloader STATIC
  bootloader.cpp
  bootloader_data.cpp
  firmware_archive.cpp
  ${LIBTINYXML2_SRC}
)

target_include_directories(bootloader PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include" # This path points to pololu-tic-software/include
    # Add other necessary include paths here if string_to_int.h is elsewhere, e.g.:
    # "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    # "${CMAKE_CURRENT_BINARY_DIR}/../include" # If generated headers are needed for bootloader
)

set_source_files_properties(libtinyxml2/tinyxml2.cpp PROPERTIES COMPILE_FLAGS -Wno-implicit-fallthrough)

set_property (TARGET bootloader PROPERTY
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})

set_property (TARGET bootloader PROPERTY
  INTERFACE_COMPILE_OPTIONS ${LIBUSBP_CFLAGS})

target_link_libraries (bootloader "${LIBUSBP_LDFLAGS_STR}")

