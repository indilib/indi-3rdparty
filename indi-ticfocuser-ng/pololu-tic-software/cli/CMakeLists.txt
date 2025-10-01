use_cxx11()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

configure_file (cli_info.rc.in cli_info.rc)

add_executable (cli
  cli.cpp
  print_status.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/cli_info.rc
)

set_target_properties (cli PROPERTIES
  OUTPUT_NAME ${CLI_NAME}
)

include_directories (
  "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries (cli lib)

install(TARGETS cli DESTINATION bin)
