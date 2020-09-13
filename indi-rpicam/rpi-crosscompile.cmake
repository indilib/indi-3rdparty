#
# Crosscompilation file for cross-compiling indi-rpicam for Raspberry PI
# Use with cmake and add -DCMAKE_TOOLCHAIN_FILE=path/to/this-file
#
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armhf)

set(CMAKE_STAGING_PREFIX /home/lasse/git/indi-cross)

set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)

set(CMAKE_SYSROOT ${CMAKE_STAGING_PREFIX}/usr/lib/gcc-cross/arm-linux-gnueabihf/7)

set(CMAKE_FIND_ROOT_PATH
    /home/lasse/git/indi-cross
)

set(CMAKE_INCLUDE_PATH ${CMAKE_STAGING_PREFIX}/usr/include)

set(CMAKE_LIBRARY_PATH ${CMAKE_STAGING_PREFIX}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE})

set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
