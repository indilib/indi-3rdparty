# - Try to find ffmpeg libraries (libavcodec, libavdevice, libavformat, libavutil, and libswscale)
# Once done this will define
#
# FFMPEG_FOUND - system has ffmpeg or libav
# FFMPEG_INCLUDE_DIR - the ffmpeg include directory
# FFMPEG_LIBRARIES - Link these to use ffmpeg
# FFMPEG_LIBAVCODEC
# FFMPEG_LIBAVDEVICE
# FFMPEG_LIBAVFORMAT
# FFMPEG_LIBAVUTIL
# FFMPEG_LIBSWSCALE
#
# Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
# Modified for other libraries by Lasse Kärkkäinen <tronic>
# Modified for Hedgewars by Stepik777
# Modified for INDILIB by rlancaste
#
# Redistribution and use is allowed according to the terms of the New
# BSD license.
#

macro(_FFMPEG_PACKAGE_check_version)
	if(EXISTS "${PACKAGE_INCLUDE_DIR}/version.h")
		file(READ "${PACKAGE_INCLUDE_DIR}/version.h" _FFMPEG_PACKAGE_version_header)

		string(REGEX MATCH "#define ${PACKAGE_NAME}_VERSION_MAJOR[ \t]+([0-9]+)" _VERSION_MAJOR_match "${_FFMPEG_PACKAGE_version_header}")
		set(FFMPEG_PACKAGE_VERSION_MAJOR "${CMAKE_MATCH_1}")
		string(REGEX MATCH "#define ${PACKAGE_NAME}_VERSION_MINOR[ \t]+([0-9]+)" _VERSION_MINOR_match "${_FFMPEG_PACKAGE_version_header}")
		set(FFMPEG_PACKAGE_VERSION_MINOR "${CMAKE_MATCH_1}")
		string(REGEX MATCH "#define ${PACKAGE_NAME}_VERSION_MICRO[ \t]+([0-9]+)" _VERSION_MICRO_match "${_FFMPEG_PACKAGE_version_header}")
		set(FFMPEG_PACKAGE_VERSION_MICRO "${CMAKE_MATCH_1}")

		set(FFMPEG_PACKAGE_VERSION ${FFMPEG_PACKAGE_VERSION_MAJOR}.${FFMPEG_PACKAGE_VERSION_MINOR}.${FFMPEG_PACKAGE_VERSION_MICRO})
		if(${FFMPEG_PACKAGE_VERSION} VERSION_LESS ${FFMPEG_PACKAGE_FIND_VERSION})
			set(FFMPEG_PACKAGE_VERSION_OK FALSE)
		else(${FFMPEG_PACKAGE_VERSION} VERSION_LESS ${FFMPEG_PACKAGE_FIND_VERSION})
			set(FFMPEG_PACKAGE_VERSION_OK TRUE)
		endif(${FFMPEG_PACKAGE_VERSION} VERSION_LESS ${FFMPEG_PACKAGE_FIND_VERSION})

		if(NOT FFMPEG_PACKAGE_VERSION_OK)
			message(STATUS "${PACKAGE_NAME} version ${FFMPEG_PACKAGE_VERSION} found in ${PACKAGE_INCLUDE_DIR}, "
					"but at least version ${FFMPEG_PACKAGE_FIND_VERSION} is required")
		else(NOT FFMPEG_PACKAGE_VERSION_OK)
			mark_as_advanced(FFMPEG_PACKAGE_VERSION_MAJOR FFMPEG_PACKAGE_VERSION_MINOR FFMPEG_PACKAGE_VERSION_MICRO)
		endif(NOT FFMPEG_PACKAGE_VERSION_OK)
    else(EXISTS "${PACKAGE_INCLUDE_DIR}/version.h")
    	set(FFMPEG_PACKAGE_VERSION_OK FALSE)
    	message(STATUS "${PACKAGE_NAME}'s version.h file was not found in the include directory: ${PACKAGE_INCLUDE_DIR}, please install this program.")
    endif(EXISTS "${PACKAGE_INCLUDE_DIR}/version.h")
endmacro(_FFMPEG_PACKAGE_check_version)

# required ffmpeg library versions, Requiring at least FFMPEG 3.2.11, Hypatia
set(_avcodec_ver ">=57.64.101")
set(_avdevice_ver ">=57.1.100")
set(_avformat_ver ">=57.56.100")
set(_avutil_ver ">=55.34.100")
set(_swscale_ver ">=4.2.100")

if (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)

	# in cache already
	set(FFMPEG_FOUND TRUE)

else (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls

find_path(FFMPEG_INCLUDE_DIR
NAMES libavcodec/avcodec.h
PATHS ${FFMPEG_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/include /usr/include /usr/local/include /opt/local/include /sw/include
PATH_SUFFIXES ffmpeg libav
)

find_package(PkgConfig)
if (PKG_CONFIG_FOUND)

	pkg_check_modules(AVCODEC libavcodec${_avcodec_ver})
	pkg_check_modules(AVDEVICE libavdevice${_avdevice_ver})
	pkg_check_modules(AVFORMAT libavformat${_avformat_ver})
	pkg_check_modules(AVUTIL libavutil${_avutil_ver})
	pkg_check_modules(SWSCALE libswscale${_swscale_ver})

endif (PKG_CONFIG_FOUND)

if (NOT PKG_CONFIG_FOUND OR 
	NOT FFMPEG_LIBAVCODEC OR
	NOT FFMPEG_LIBAVDEVICE OR
	NOT FFMPEG_LIBAVFORMAT OR
	NOT FFMPEG_LIBAVUTIL OR
	NOT FFMPEG_LIBSWSCALE)

# LIBAVCODEC
	set(PACKAGE_NAME "LIBAVCODEC")
	set(PACKAGE_INCLUDE_DIR "${FFMPEG_INCLUDE_DIR}/libavcodec")
	set(FFMPEG_PACKAGE_FIND_VERSION _avcodec_ver)
	
	_FFMPEG_PACKAGE_check_version()
	
	if(FFMPEG_PACKAGE_VERSION_OK)
		set(AVCODEC_VERSION FFMPEG_PACKAGE_VERSION)
	endif(FFMPEG_PACKAGE_VERSION_OK)
	
# LIBAVDEVICE
	set(PACKAGE_NAME "LIBAVDEVICE")
	set(PACKAGE_INCLUDE_DIR "${FFMPEG_INCLUDE_DIR}/libavdevice")
	set(FFMPEG_PACKAGE_FIND_VERSION _avdevice_ver)
	
	_FFMPEG_PACKAGE_check_version()
	
	if(FFMPEG_PACKAGE_VERSION_OK)
		set(AVDEVICE_VERSION FFMPEG_PACKAGE_VERSION)
	endif(FFMPEG_PACKAGE_VERSION_OK)
	
# LIBAVFORMAT
	set(PACKAGE_NAME "LIBAVFORMAT")
	set(PACKAGE_INCLUDE_DIR "${FFMPEG_INCLUDE_DIR}/libavformat")
	set(FFMPEG_PACKAGE_FIND_VERSION _avformat_ver)
	
	_FFMPEG_PACKAGE_check_version()
	
	if(FFMPEG_PACKAGE_VERSION_OK)
		set(AVFORMAT_VERSION FFMPEG_PACKAGE_VERSION)
	endif(FFMPEG_PACKAGE_VERSION_OK)
	
# LIBAVUTIL
	set(PACKAGE_NAME "LIBAVUTIL")
	set(PACKAGE_INCLUDE_DIR "${FFMPEG_INCLUDE_DIR}/libavutil")
	set(FFMPEG_PACKAGE_FIND_VERSION _avutil_ver)
	
	_FFMPEG_PACKAGE_check_version()
	
	if(FFMPEG_PACKAGE_VERSION_OK)
		set(AVUTIL_VERSION FFMPEG_PACKAGE_VERSION)
	endif(FFMPEG_PACKAGE_VERSION_OK)

# LIBSWSCALE
	set(PACKAGE_NAME "LIBSWSCALE")
	set(PACKAGE_INCLUDE_DIR "${FFMPEG_INCLUDE_DIR}/libswscale")
	set(FFMPEG_PACKAGE_FIND_VERSION _swscale_ver)
	
	_FFMPEG_PACKAGE_check_version()
	
	if(FFMPEG_PACKAGE_VERSION_OK)
		set(SWSCALE_VERSION FFMPEG_PACKAGE_VERSION)
	endif(FFMPEG_PACKAGE_VERSION_OK)
	
endif ()

find_library(FFMPEG_LIBAVCODEC
NAMES avcodec libavcodec
PATHS ${AVCODEC_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}/lib /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVDEVICE
NAMES avdevice libavdevice
PATHS ${AVDEVICE_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}/lib /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVFORMAT
NAMES avformat libavformat
PATHS ${AVFORMAT_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}/lib /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVUTIL
NAMES avutil libavutil
PATHS ${AVUTIL_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}/lib /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBSWSCALE
NAMES swscale libswscale
PATHS ${SWSCALE_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}/lib /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

#Only set FFMPEG to found if all the libraries are found in the right versions.
if(AVCODEC_VERSION AND
                 AVDEVICE_VERSION AND
                 AVFORMAT_VERSION AND
                 AVUTIL_VERSION AND
                 SWSCALE_VERSION AND
				 FFMPEG_LIBAVCODEC AND
				 FFMPEG_LIBAVDEVICE AND
				 FFMPEG_LIBAVFORMAT AND
				 FFMPEG_LIBAVUTIL AND
				 FFMPEG_LIBSWSCALE)
set(FFMPEG_FOUND TRUE)
endif()

if (FFMPEG_FOUND)

	set(FFMPEG_LIBRARIES
	${FFMPEG_LIBAVCODEC}
	${FFMPEG_LIBAVDEVICE}
	${FFMPEG_LIBAVFORMAT}
	${FFMPEG_LIBAVUTIL}
	${FFMPEG_LIBSWSCALE}
	)

endif (FFMPEG_FOUND)

if (FFMPEG_FOUND)
	if (NOT FFMPEG_FIND_QUIETLY)
		message(STATUS "Found FFMPEG: ${FFMPEG_LIBRARIES}, ${FFMPEG_INCLUDE_DIR}")
	endif (NOT FFMPEG_FIND_QUIETLY)
	else (FFMPEG_FOUND)
		message(STATUS "Could not find up to date FFMPEG for INDI Webcam. Up to date versions of these packages are required: libavcodec, libavdevice, libavformat, libavutil, and libswscale")
	if (FFMPEG_FIND_REQUIRED)
		message(FATAL_ERROR "Error: FFMPEG is required by this package!")
	endif (FFMPEG_FIND_REQUIRED)
endif (FFMPEG_FOUND)

endif (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)


