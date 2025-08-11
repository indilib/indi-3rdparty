cmake_minimum_required (VERSION 3.10.0)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Find libusbp using pkg-config and use it.
pkg_check_modules(LIBUSBP QUIET libusbp-1)
string (REPLACE ";" " " LIBUSBP_CFLAGS "${LIBUSBP_CFLAGS}")
string (REPLACE ";" " " LIBUSBP_LDFLAGS "${LIBUSBP_LDFLAGS}")
if (NOT BUILD_SHARED_LIBS)
  set (PC_REQUIRES "${PC_REQUIRES} libusbp-1")
endif ()

# Try to find system libyaml
pkg_check_modules(YAML yaml)

if (YAML_FOUND)
  message(STATUS "Found system YAML (yaml) library.")
  string (REPLACE ";" " " LIBYAML_CFLAGS "${YAML_CFLAGS}")
  string (REPLACE ";" " " LIBYAML_LDFLAGS "${YAML_LDFLAGS}")
  if (NOT BUILD_SHARED_LIBS)
    set (PC_REQUIRES "${PC_REQUIRES} yaml")
  endif ()
else ()
  message(STATUS "System YAML (yaml) not found. Building bundled libyaml.")
  set (LIBYAML_SRC libyaml/yaml.c)
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/libyaml")
  set (LIBYAML_CFLAGS "-DYAML_DECLARE_STATIC")
endif ()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBUSBP_CFLAGS} ${LIBYAML_CFLAGS}")

# Settings for GCC
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # By default, symbols are not visible outside of the library.
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif ()

# If we are building a DLL, put version and name information in it.
set (os_src ${CMAKE_CURRENT_BINARY_DIR}/lib_info.rc)

add_library (lib
  tic_baud_rate.c
  tic_current_limit.c
  tic_device.c
  tic_get_settings.c
  tic_set_settings.c
  tic_error.c
  tic_handle.c
  tic_names.c
  tic_settings.c
  tic_settings_fix.c
  tic_settings_read_from_string.c
  tic_settings_to_string.c
  tic_string.c
  tic_variables.c
  ${os_src}
  ${LIBYAML_SRC}
)

target_include_directories(lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"             # For tic.h, tic_internal.h, etc.
    "${CMAKE_CURRENT_BINARY_DIR}/../include"             # For the generated config.h
)

set_target_properties(lib PROPERTIES
  OUTPUT_NAME pololu-tic-${SOFTWARE_VERSION_MAJOR}
  SOVERSION ${SOFTWARE_VERSION}
  VERSION ${SOFTWARE_VERSION}
  DEFINE_SYMBOL TIC_EXPORTS
)

configure_file (
  "lib_info.rc.in"
  "lib_info.rc"
)

set_target_properties(lib PROPERTIES
  OUTPUT_NAME pololu-tic-${SOFTWARE_VERSION_MAJOR}
  SOVERSION ${SOFTWARE_VERSION}
  VERSION ${SOFTWARE_VERSION}
  DEFINE_SYMBOL TIC_EXPORTS
)

target_link_libraries (lib
  PUBLIC usbp # <--- Ajout de la cible CMake 'usbp' que vous construisez localement
  # Les LDFLAGS du pkg-config ne sont généralement pas nécessaires si nous lions à une cible
  # mais les garder ne devrait pas nuire si la cible 'usbp' ne les propage pas automatiquement.
  # PRIVATE "${LIBUSBP_LDFLAGS}" # Mettre en PRIVATE si usbp n'est pas une dépendance transitive
  PRIVATE "${LIBYAML_LDFLAGS}"
)

configure_file (
  "lib.pc.in"
  "libpololu-tic-${SOFTWARE_VERSION_MAJOR}.pc"
  @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libpololu-tic-${SOFTWARE_VERSION_MAJOR}.pc"
  DESTINATION lib/pkgconfig)

install(TARGETS lib
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
