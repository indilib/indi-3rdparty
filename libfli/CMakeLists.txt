cmake_minimum_required(VERSION 3.16)
PROJECT(libfli CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(InstallImported)

option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

ADD_DEFINITIONS(-Wall -O2 -D__LIBUSB__)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

find_package(USB1 REQUIRED)
find_package(INDI REQUIRED)

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

include_directories( ${INDI_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${USB1_INCLUDE_DIR})
include_directories(unix)

if (APPLE)
    ##This one is needed for homebrew
include_directories( "/usr/local/include")
    ## This one is needed for Craft
include_directories("${CMAKE_INSTALL_PREFIX}/include")
endif(APPLE)

add_library(flipro SHARED IMPORTED)
add_library(flialgo SHARED IMPORTED)
set (FLIPRO_VERSION "2.1.4")
set (FLIPRO_SOVERSION "2")

if (UNIX AND NOT WIN32)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set_property (TARGET flipro PROPERTY IMPORTED_LOCATION "flipro/arm64/libflipro.bin")
    set_property (TARGET flialgo PROPERTY IMPORTED_LOCATION "flipro/arm64/libflialgo.bin")
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set_property (TARGET flipro PROPERTY IMPORTED_LOCATION "flipro/x64/libflipro.bin")
    set_property (TARGET flialgo PROPERTY IMPORTED_LOCATION "flipro/x64/libflialgo.bin")
  endif ()
endif ()

set_target_properties(flipro PROPERTIES VERSION ${FLIPRO_VERSION} SOVERSION ${FLIPRO_SOVERSION})
set_target_properties(flialgo PROPERTIES VERSION ${FLIPRO_VERSION} SOVERSION ${FLIPRO_SOVERSION})

set(fli_LIB_SRCS
   libfli.c   
   libfli-camera.c   
   libfli-camera-parport.c   
   libfli-camera-usb.c   
   libfli-filter-focuser.c   
   libfli-mem.c
   libfli-raw.c
      
   unix/libfli-usb.c
   unix/libfli-debug.c
   unix/libfli-serial.c
   unix/libfli-sys.c
   
   #unix/linux/libfli-usb-sys.c
   
   # LIBUSB support
   unix/libusb/libfli-usb-sys.c
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
list(APPEND fli_LIB_SRCS unix/linux/libfli-parport.c)
endif()

#build a shared library
ADD_LIBRARY(fli SHARED ${fli_LIB_SRCS})

set_target_properties(fli PROPERTIES VERSION 2.0 SOVERSION 2)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(fli ${USB1_LIBRARIES} -lm -lpthread)

#add an install target here
INSTALL(FILES libfli.h flipro/libflipro.h DESTINATION include)

INSTALL(TARGETS fli LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install library
install_imported (
  TARGETS flipro flialgo
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND INDI_INSTALL_UDEV_RULES)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-fli.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
