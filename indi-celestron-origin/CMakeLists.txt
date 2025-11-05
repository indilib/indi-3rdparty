cmake_minimum_required(VERSION 3.16)
project(celestron_origin CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core Network Gui REQUIRED)  # Removed WebSockets
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)  # ADD for base64 encoding
find_package(TIFF REQUIRED)

# --------------------------------------------------------------------
# Find libindi via pkg-config
# --------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(INDI REQUIRED libindi)

if(NOT INDI_LIBRARIES)
    message(WARNING "pkg-config did not provide INDI_LIBRARIES, setting manually to 'indi'")
    set(INDI_LIBRARIES indidriver)
endif()

message(STATUS "")
message(STATUS "================= INDI CONFIGURATION =================")
message(STATUS "INDI_FOUND:          ${INDI_FOUND}")
message(STATUS "INDI_VERSION:        ${INDI_VERSION}")
message(STATUS "INDI_INCLUDE_DIRS:   ${INDI_INCLUDE_DIRS}")
message(STATUS "INDI_LIBRARY_DIRS:   ${INDI_LIBRARY_DIRS}")
message(STATUS "INDI_LIBRARIES:      ${INDI_LIBRARIES}")
message(STATUS "INDI_CFLAGS:         ${INDI_CFLAGS}")
message(STATUS "INDI_CFLAGS_OTHER:   ${INDI_CFLAGS_OTHER}")
message(STATUS "======================================================")
message(STATUS "")

# Add include and library directories discovered by pkg-config
include_directories(${INDI_INCLUDE_DIRS})
link_directories(${INDI_LIBRARY_DIRS})

# --------------------------------------------------------------------
# Your driver
# --------------------------------------------------------------------
add_executable(indi-celestron-origin
indi_origin.cpp OriginBackendSimple.cpp SimpleWebSocket.cpp TelescopeDataProcessor.cpp
)

# Link against libindi
target_link_libraries(indi-celestron-origin PRIVATE ${INDI_LIBRARIES} ${TIFF_LIBRARIES} Qt5::Core Qt5::Network)
target_compile_options(indi-celestron-origin PRIVATE ${INDI_CFLAGS_OTHER})

# --------------------------------------------------------------------
# Install
# --------------------------------------------------------------------
install(TARGETS indi-celestron-origin RUNTIME DESTINATION bin)
install(FILES indi_celestron_origin.xml DESTINATION share/indi)
