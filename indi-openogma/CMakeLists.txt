cmake_minimum_required(VERSION 3.10)
project(indi-openogma C CXX)

# Install layout helpers and (optional) shared cmake from the 3rdparty tree
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)

# INDI headers use std::any → require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find INDI from the 3rd-party superbuild (required)
find_package(INDI REQUIRED)

# Driver version for config.h / XML templating
set(CDRIVER_VERSION_MAJOR 1)
set(CDRIVER_VERSION_MINOR 2)

# Generate config.h and XML from the provided templates
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  @ONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_openogma.xml.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/indi_openogma.xml
  @ONLY
)

# Add the driver binary
add_executable(indi_openogma
  indi_openogma.cpp
)

# Enforce C++17 on this target (robust against cache/toolchain quirks)
target_compile_features(indi_openogma PRIVATE cxx_std_17)

# Includes (scoped to target)
target_include_directories(indi_openogma PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${INDI_INCLUDE_DIR}
)

# Link to INDI (support both modern imported target and legacy vars)
if (TARGET INDI::Driver)
  target_link_libraries(indi_openogma PRIVATE INDI::Driver)
else()
  target_link_libraries(indi_openogma PRIVATE ${INDI_DRIVER_LIBRARIES})
endif()

# Install the driver into the usual location (respects CMAKE_INSTALL_PREFIX)
install(TARGETS indi_openogma RUNTIME DESTINATION bin COMPONENT openogma)

# Install the generated XML so clients (Ekos) can load the driver
# Be resilient if INDI_DATA_DIR isn't defined by the superbuild
set(_INDI_DATA_DIR "${INDI_DATA_DIR}")
if (NOT _INDI_DATA_DIR)
  set(_INDI_DATA_DIR "${CMAKE_INSTALL_DATADIR}/indi")
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_openogma.xml
        DESTINATION ${_INDI_DATA_DIR} COMPONENT openogma)

# Install udev rules
if (UNIX AND NOT WIN32 AND INDI_INSTALL_UDEV_RULES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/99-openogma.rules")
  install(FILES 99-openogma.rules DESTINATION /lib/udev/rules.d)
endif()

