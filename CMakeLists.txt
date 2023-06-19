cmake_minimum_required(VERSION 3.0)

PROJECT(libindi-3rdparty CXX C)
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

include(GNUInstallDirs)

## Some files like libnova.h and libusb.h are in in subdirectories of the include directory
## For the CMAKE Modules, they find the subdirectory, so then something like ln_types.h should be #include ln_types.h
## But some packages and drivers write their header files like this: #include libnova/ln_types.h
## On Linux, this is fine since the top include directory such as /usr/include is already included and therefore
## <libnova/ln_types.h> is resolved. But on Mac it its not already in the path and has to be explicitly added.

if (APPLE)
    ##This one is needed for homebrew
include_directories( "/usr/local/include")
    ## This one is needed for Craft
include_directories("${CMAKE_INSTALL_PREFIX}/include")

# FIX_LIBRARY_LINK Macro
# This macro will check MacOS Binary Libraries provided by third parties for external links that could cause linking and packaging problems 
# If the FIX_MACOS_LIBS option is enabled when building, it will also attempt to fix the problems, which should then allow the build to continue.
# This macro should be called by the FIX_MACOS_LIBRARIES macro below
macro(FIX_LIBRARY_LINK PACKAGE_NAME CURRENT_DYLIB_FILENAME LIBRARY_DISPLAY_NAME LINK)
	if("${LINK}" MATCHES "${PACKAGE_NAME}")
		if (NOT "${LINK}" STREQUAL "@rpath/${PACKAGE_NAME}.dylib")
			if (FIX_MACOS_LIBS)
				execute_process(COMMAND bash "-c" "install_name_tool -change ${LINK} @rpath/${PACKAGE_NAME}.dylib ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME}")
				message(WARNING "The mac library ${CURRENT_DYLIB_FILENAME} in the ${LIBRARY_DISPLAY_NAME} source folder has a link to ${PACKAGE_NAME}.dylib that says ${LINK}. "
					"The issue is now fixed by changing the link to @rpath/${PACKAGE_NAME}.dylib for better compatibility."
					"Please push the changes to INDI 3rd party repository if you have write access, make a pull request, or let the maintainer know to fix the issue. "
				)
			else(FIX_MACOS_LIBS)
				message (FATAL_ERROR
					"The mac library ${CURRENT_DYLIB_FILENAME} in the ${LIBRARY_DISPLAY_NAME} source folder has a link to ${PACKAGE_NAME}.dylib that says ${LINK}. "
					"This could cause issues if ${PACKAGE_NAME}.dylib is not actually at that location. "
					"If you would like to attempt to fix the problem on your machine and continue the build, please enable the FIX_MACOS_LIBS option. "
				)
			endif(FIX_MACOS_LIBS)
		endif (NOT "${LINK}" STREQUAL "@rpath/${PACKAGE_NAME}.dylib")
    endif("${LINK}" MATCHES "${PACKAGE_NAME}")
endmacro(FIX_LIBRARY_LINK)


# FIX_LIBRARY_ID Macro
# This macro will check MacOS Binary Libraries provided by third parties for Install IDs that cold cause linking and packaging problems.
# If the FIX_MACOS_LIBS option is enabled when building, it will also attempt to fix the problems, which should then allow the build to continue.
# This macro should be called by the FIX_MACOS_LIBRARIES macro below
macro(FIX_LIBRARY_ID DYLIB_NAME CURRENT_DYLIB_FILENAME LIBRARY_DISPLAY_NAME)
	execute_process(COMMAND bash "-c" "otool -D ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME} | tail -n 1 | tr -d '\n'" OUTPUT_VARIABLE DYLIB_ID)
     if (NOT ${DYLIB_ID} STREQUAL "@rpath/${DYLIB_NAME}.dylib")
         if (FIX_MACOS_LIBS)
             execute_process(COMMAND bash "-c" "install_name_tool -id @rpath/${DYLIB_NAME}.dylib ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME}")
             message(WARNING  "The Mac library ${DYLIB_NAME}.dylib in the ${LIBRARY_DISPLAY_NAME} source folder had the ID ${DYLIB_ID} which could cause linking problems. "
             	"The issue is now fixed by changing the library ${DYLIB_NAME}.dylib to have the id @rpath/${DYLIB_NAME}.dylib so it links properly with rpaths. "
            	"Please push the changes to INDI 3rd party repository if you have write access, make a pull request, or let the maintainer know to fix the issue. "
             )
         else(FIX_MACOS_LIBS)
             message (FATAL_ERROR
                 "The Mac library ${DYLIB_NAME}.dylib in the ${LIBRARY_DISPLAY_NAME} source folder has the ID ${DYLIB_ID} which could cause linking problems. "
                 "Please let the maintainer for ${LIBRARY_DISPLAY_NAME} know about the issue."
                 "If you would like to attempt to fix the problem on your machine and continue the build, please enable the FIX_MACOS_LIBS option. "
             )
     	endif(FIX_MACOS_LIBS)
     endif (NOT ${DYLIB_ID} STREQUAL "@rpath/${DYLIB_NAME}.dylib")
endmacro(FIX_LIBRARY_ID)

# FIX_MACOS_LIBRARIES Macro
# On MacOS, the Install ID of the library is used for linking with the programs that need them.
# If a version number is used in the install ID, linking could fail because sometimes the numbers don't match.  (Example: @rpath/libqhyccd.20.dylib)
# If @loader_path, @executable_path, or an absolute path are used and the library is installed or packaged differently than that,
# linking could fail (Example: @loader_path/libASICamera2.dylib or /usr/local/lib/libsbig.dylib).
# It is easiest to make the Install ID with just rpath and the name of the library (Examples: @rpath/libqhyccd.dylib and @rpath/libASICamera2.dylib).
# Then when linking, it will use that path for linking, and the program just has to have the right rpath.
# It will not matter then if the library is located in /usr/local/lib or bundled in an app bundle.
# This code will check the library for these errors, correct them or give warnings asking that they get fixed.
# The warnings do have to be fatal errors because if this problem does not get fixed, it will appear to be right, but linking will have failed and it won't work.

macro(FIX_MACOS_LIBRARIES DYLIB_NAME CURRENT_DYLIB_FILENAME LIBRARY_DISPLAY_NAME)

	 # This section will use the FIX_LIBRARY_ID macro to check and optionally fix Install IDs that could cause linking issues.
	 FIX_LIBRARY_ID(${DYLIB_NAME} ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME})
     
     # This section gets all the current links to other libraries then checks and optionally corrects absolute or bad links to libraries including libusb and opencv
	 execute_process(COMMAND bash "-c" "otool -L ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME} | cut -d ' ' -f1" OUTPUT_VARIABLE LINKS_LIST)
	 string (REPLACE "\t" "" LINKS_LIST "${LINKS_LIST}")
	 string (REPLACE "\n" ";" LINKS_LIST "${LINKS_LIST}")
	 foreach(LINK IN LISTS LINKS_LIST)
	 	if(NOT ${LINK} STREQUAL "")
	 		FIX_LIBRARY_LINK("libusb-1.0.0" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
			FIX_LIBRARY_LINK("libopencv_highgui" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
			FIX_LIBRARY_LINK("libopencv_videoio" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
			FIX_LIBRARY_LINK("libopencv_imgcodecs" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
			FIX_LIBRARY_LINK("libopencv_imgproc" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
			FIX_LIBRARY_LINK("libopencv_core" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
		endif(NOT ${LINK} STREQUAL "")
	 endforeach(LINK IN LISTS LINKS_LIST)
  
endmacro(FIX_MACOS_LIBRARIES)

endif(APPLE)

set(LIBRARIES_FOUND TRUE)

include(CMakeCommon)

# Clang Format support
IF (UNIX OR APPLE)
    SET(FORMAT_CODE OFF CACHE BOOL "Enable Clang Format")
    IF (FORMAT_CODE MATCHES ON)
        FILE(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

        FOREACH(SOURCE_FILE ${ALL_SOURCE_FILES})
            STRING(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)
            IF (NOT ${DIR_FOUND} EQUAL 0)
                LIST(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            ENDIF ()
            # Don't apply the format for 3rd party libraries
            STRING(FIND ${SOURCE_FILE} libapogee DIR1_FOUND)
            STRING(FIND ${SOURCE_FILE} libfishcamp DIR2_FOUND)
            STRING(FIND ${SOURCE_FILE} libfli DIR3_FOUND)
            STRING(FIND ${SOURCE_FILE} libqhy DIR4_FOUND)
            STRING(FIND ${SOURCE_FILE} libqsi DIR5_FOUND)
            STRING(FIND ${SOURCE_FILE} libsbig DIR6_FOUND)            
            STRING(FIND ${SOURCE_FILE} libinovasdk DIR8_FOUND)
            STRING(FIND ${SOURCE_FILE} libsvbony DIR9_FOUND)
            STRING(FIND ${SOURCE_FILE} libahp-xc DIR10_FOUND)
            STRING(FIND ${SOURCE_FILE} libahp-gt DIR11_FOUND)
            IF (NOT ${DIR1_FOUND} EQUAL -1 OR NOT ${DIR2_FOUND} EQUAL -1 OR NOT ${DIR3_FOUND} EQUAL -1 OR NOT ${DIR4_FOUND} EQUAL -1 OR
                NOT ${DIR5_FOUND} EQUAL -1 OR NOT ${DIR6_FOUND} EQUAL -1 OR NOT ${DIR7_FOUND} EQUAL -1 OR NOT ${DIR8_FOUND} EQUAL -1 OR
                NOT ${DIR9_FOUND} EQUAL -1 OR NOT ${DIR10_FOUND} EQUAL -1 OR NOT ${DIR11_FOUND} EQUAL -1)
                LIST(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            ENDIF ()
        ENDFOREACH ()

        FIND_PROGRAM(CLANGFORMAT_EXE NAMES clang-format-5.0)
        IF (CLANGFORMAT_EXE)
            ADD_CUSTOM_TARGET(clang-format COMMAND ${CLANGFORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES})
        ENDIF ()
    ENDIF ()
ENDIF ()

# Option to build the 3rd Party libraries instead of the 3rd Party drivers.
# This is by default OFF, so you must set the option to build them.
# It is a good idea to run with this option before the 3rd Party build so all the libraries are built first.
option(BUILD_LIBS "Build 3rd Party Libraries, not 3rd Party Drivers" Off)

# Fix MacOS libraries linking
# Do not ENABLE on production! Should be enabled by INDI core developers only
# to fix the affected libraries and push a fix to the repo after a binary update.
option(FIX_MACOS_LIBS "Fix MacOS Libraries links" Off)

# Define standard set of drivers to build (default linux target)
option(WITH_EQMOD "Install EQMod Driver" On)
option(WITH_STARBOOK "Install Starbook Driver" On)
option(WITH_STARBOOK_TEN "Install Starbook Ten Driver" On)
option(WITH_CAUX "Install Celestron AUX Driver" On)
option(WITH_SX "Install StarLight Xpress Driver" On)
option(WITH_MAXDOME "Install MaxDomeII Driver" On)
option(WITH_NEXDOME "Install NexDome Driver" On)
option(WITH_SPECTRACYBER "Install Spectracyber Driver" On)
option(WITH_CLOUDWATCHER "Install AAG Cloud Watcher Driver" On)
option(WITH_MI "Install Moravian Driver" On)
option(WITH_FLI "Install FLI Driver" On)
option(WITH_SBIG "Install SBIG Driver" On)
option(WITH_INOVAPLX "Install i.Nova PLx Driver" On)
option(WITH_APOGEE "Install Apogee Driver" On)
option(WITH_FFMV "Install Point Grey FireFly MV Driver" On)
option(WITH_QHY "Install QHY Driver" On)
option(WITH_GPHOTO "Install GPhoto Driver" On)
option(WITH_QSI "Install QSI Driver" On)
option(WITH_DUINO "Install Ariduino Driver" On)
option(WITH_FISHCAMP "Install Fishcamp Driver" On)
option(WITH_GPSD "Install GPSD Driver" On)
option(WITH_GIGE "Install GiGE machine vision Driver" Off)
option(WITH_DSI "Install Meade DSI Driver" On)
option(WITH_ASICAM "Install ZWO Optics ASI Driver" On)
option(WITH_MGEN "Install MGen Autoguider" On)
option(WITH_ASTROMECHFOC "Install Astromechanics Focuser" On)
option(WITH_LIMESDR "Install LIME-SDR Receiver" On)
option(WITH_RADIOSIM "Install RadioSim Receiver" On)
option(WITH_GPSNMEA "Install GPS NMEA Driver" On)
option(WITH_RTKLIB "Install RTKLIB Driver" On)
option(WITH_ARMADILLO "Install Armadillo & Platypus Driver" On)
option(WITH_NIGHTSCAPE "Install Nightscape 8300 Driver" On)
option(WITH_ATIK "Install Atik Driver" On)
option(WITH_TOUPBASE "Install Toupbase Driver" On)
option(WITH_DREAMFOCUSER "Install DreamFocuser Driver" On)
option(WITH_AVALON "Install Avalon StarGO Driver" On)
option(WITH_BEEFOCUS "Install Bee Focuser Driver" On)
option(WITH_SHELYAK "Install Shelyak Spectrograph Driver" On)
option(WITH_SKYWALKER "Install AOK SkyWalker Mount Driver" On)
option(WITH_TALON6 "Install Talon6 Mount Driver" On)
option(WITH_PENTAX "Install Pentax Driver" On)
option(WITH_ASTROLINK4 "Install AstroLink4 Driver" On)
option(WITH_AHP_XC "Install AHP XC Correlators Driver" On)
option(WITH_AHP_GT "Install AHP GT Controllers Driver" On)
option(WITH_ORION_SSG3 "Install Orion StarShoot G3 Driver" On)
option(WITH_SVBONY "Install SVBONY Camera Driver" On)
option(WITH_BRESSEREXOS2 "Install Bresser Exos 2 GoTo Mount Driver" On)
option(WITH_PLAYERONE "Install Player One Astronomy's Camera Driver" On)
option(WITH_WEEWX_JSON "Install Weewx JSON Driver" On)
option(WITH_ROLLOFFINO "Install RollOff ino Dome Driver" On)
option(WITH_ASTROASIS "Install Astroasis Driver" On)

# FFMPEG required for INDI Webcam driver
find_package(FFmpeg)
if (FFMPEG_FOUND)
    message(STATUS "Since FFMPEG was found, INDI Webcam Driver can be built")
    option(WITH_WEBCAM "Install INDI Webcam Driver based on FFMPEG" On)
ELSE(FFMPEG_FOUND)
    message(STATUS "Since an up to date FFMPEG was not found, INDI Webcam Driver will not be built")
    option(WITH_WEBCAM "Install INDI Webcam Driver based on FFMPEG" Off)
ENDIF(FFMPEG_FOUND)


# MMAL Required for Raspberry PI camera driver
if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
find_package(MMAL)
endif()

IF (MMAL_FOUND)
option(WITH_RPICAM "Install Raspberry Pi High Quality Camera Driver" On)
ELSE ()
option(WITH_RPICAM "Install Raspberry Pi High Quality Camera Driver" Off)
ENDIF(MMAL_FOUND)

find_package(NUTClient)
if (NUTCLIENT_FOUND)
    message(STATUS "Since nutclient was found, INDI NUT Driver can be built")
    option(WITH_NUT "Install INDI NUT Driver" On)
ELSE(NUTCLIENT_FOUND)
    message(STATUS "Since an up to date nutclient was not found, INDI NUT Driver will not be built")
    option(WITH_NUT "Install INDI NUT Driver" Off)
ENDIF(NUTCLIENT_FOUND)

# Add/remove cases for OSX
IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(WITH_INOVAPLX Off)
    set(WITH_GIGE Off)    
    set(WITH_PENTAX Off)
    set(WITH_GPSD Off)
    set(WITH_AHP_XC Off)
    set(WITH_AHP_GT Off)
    set(WITH_ASTROASIS Off)
ENDIF ()
# Disable apogee, qhy and mi with gcc 4.8 and earlier versions
IF (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
    SET(WITH_APOGEE Off)
    SET(WITH_QHY Off)
    SET(WITH_MI Off)
ENDIF ()

# Disable pre-built SDKs / drivers
option(NO_PRE_BUILT "Do not install pre-built libraries/drivers" Off)
if (NO_PRE_BUILT)
    SET(WITH_AHP_GT Off)
    SET(WITH_AHP_XC Off)
    SET(WITH_ASICAM Off)
    SET(WITH_ATIK Off)
    SET(WITH_DSI Off)
    SET(WITH_DUINO Off)
    SET(WITH_FISHCAMP Off)
    SET(WITH_INOVAPLX Off)
    SET(WITH_MI Off)
    SET(WITH_PENTAX Off)
    SET(WITH_PLAYERONE Off)
    SET(WITH_QHY Off)
    SET(WITH_QSI Off)
    SET(WITH_SBIG Off)
    SET(WITH_SVBONY Off)
    SET(WITH_TOUPBASE Off)
    SET(WITH_ASTROASIS Off)
endif(NO_PRE_BUILT)

# If the Build Libs option is selected, it will just build the required libraries.
# This should be run before the main 3rd Party Drivers build, so the drivers can find the libraries.
IF (BUILD_LIBS)

#libapogee
if (WITH_APOGEE)
add_subdirectory(libapogee)
endif(WITH_APOGEE)

#libasi
if (WITH_ASICAM)
add_subdirectory(libasi)
endif (WITH_ASICAM)

#libatik
if (WITH_ATIK)
add_subdirectory(libatik)
endif (WITH_ATIK)

#libfishcamp
if (WITH_FISHCAMP)
add_subdirectory(libfishcamp)
endif(WITH_FISHCAMP)

#libfli
if (WITH_FLI)
add_subdirectory(libfli)
endif(WITH_FLI)

#libmicam
if (WITH_MI)
add_subdirectory(libmicam)
endif(WITH_MI)

#libinovasdk
if (WITH_INOVAPLX)
add_subdirectory(libinovasdk)
endif (WITH_INOVAPLX)

#libqhy
if (WITH_QHY)
add_subdirectory(libqhy)
endif (WITH_QHY)

#libqsi
if (WITH_QSI)
add_subdirectory(libqsi)
endif (WITH_QSI)

#libsbig
if (WITH_SBIG)
add_subdirectory(libsbig)
endif (WITH_SBIG)

#libpentax
if (WITH_PENTAX)
if(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^aarch64")
add_subdirectory(libricohcamerasdk)
endif()
add_subdirectory(libpktriggercord)
endif (WITH_PENTAX)

# libahp-xc
if (WITH_AHP_XC)
add_subdirectory(libahp-xc)
endif(WITH_AHP_XC)

# libahp-gt
if (WITH_AHP_GT)
add_subdirectory(libahp-gt)
endif(WITH_AHP_GT)

#toupbase dependencies
if (WITH_TOUPBASE)
add_subdirectory(libtoupcam)
add_subdirectory(libaltaircam)
add_subdirectory(libbressercam)
add_subdirectory(libogmacam)
add_subdirectory(libstarshootg)
add_subdirectory(libnncam)
add_subdirectory(libmallincam)
add_subdirectory(libomegonprocam)
add_subdirectory(libtscam)
endif (WITH_TOUPBASE)

#libsvbiny
if (WITH_SVBONY)
add_subdirectory(libsvbony)
endif (WITH_SVBONY)

#libplayerone
if (WITH_PLAYERONE)
add_subdirectory(libplayerone)
endif(WITH_PLAYERONE)

#libastroasis
if (WITH_ASTROASIS)
add_subdirectory(libastroasis)
endif(WITH_ASTROASIS)

# This is the main 3rd Party build.  It runs if the Build Libs option is not selected.
ELSE(BUILD_LIBS)

## EQMod
if (WITH_EQMOD)
add_subdirectory(indi-eqmod)
endif(WITH_EQMOD)

## AstroLink4
if (WITH_ASTROLINK4)
add_subdirectory(indi-astrolink4)
endif(WITH_ASTROLINK4)

## SkyWalker
if (WITH_SKYWALKER)
add_subdirectory(indi-aok)
endif(WITH_SKYWALKER)

## Starbook
if (WITH_STARBOOK)
add_subdirectory(indi-starbook)
endif(WITH_STARBOOK)

## Starbook Ten
if (WITH_STARBOOK_TEN)
add_subdirectory(indi-starbook-ten)
endif(WITH_STARBOOK_TEN)

## Stalight Xpress
if (WITH_SX)
add_subdirectory(indi-sx)
endif(WITH_SX)

## Maxdome II
if (WITH_MAXDOME)
add_subdirectory(indi-maxdomeii)
endif(WITH_MAXDOME)

## NexDome
if (WITH_NEXDOME)
add_subdirectory(indi-nexdome)
endif(WITH_NEXDOME)

## Talon6
if (WITH_TALON6)
add_subdirectory(indi-talon6)
endif(WITH_TALON6)

## Rolloffino
if (WITH_ROLLOFFINO)
add_subdirectory(indi-rolloffino)
endif(WITH_ROLLOFFINO)

## SpectraCyber
if (WITH_SPECTRACYBER)
add_subdirectory(indi-spectracyber)
endif(WITH_SPECTRACYBER)

## Shelyak
if (WITH_SHELYAK)
add_subdirectory(indi-shelyak)
endif(WITH_SHELYAK)

## Cloud Watcher
if (WITH_CLOUDWATCHER)
add_subdirectory(indi-aagcloudwatcher-ng)
endif (WITH_CLOUDWATCHER)

## Celestron AUX
if (WITH_CAUX)
add_subdirectory(indi-celestronaux)
endif (WITH_CAUX)

## GPhoto
if (WITH_GPHOTO)
add_subdirectory(indi-gphoto)
endif(WITH_GPHOTO)

## QSI
if (WITH_QSI)
find_package(QSI)
if (QSI_FOUND)
add_subdirectory(indi-qsi)
else (QSI_FOUND)
add_subdirectory(libqsi)
SET(LIBRARIES_FOUND FALSE)
endif (QSI_FOUND)
endif (WITH_QSI)

## SBIG
if (WITH_SBIG)
find_package(SBIG)
if (SBIG_FOUND)
add_subdirectory(indi-sbig)
else (SBIG_FOUND)
add_subdirectory(libsbig)
SET(LIBRARIES_FOUND FALSE)
endif (SBIG_FOUND)
endif (WITH_SBIG)

## ATIK
if (WITH_ATIK)
find_package(ATIK)
if (ATIK_FOUND)
add_subdirectory(indi-atik)
else (ATIK_FOUND)
add_subdirectory(libatik)
SET(LIBRARIES_FOUND FALSE)
endif (ATIK_FOUND)
endif (WITH_ATIK)

## TOUPBASE
if (WITH_TOUPBASE)
find_package(TOUPCAM)
find_package(ALTAIRCAM)
find_package(BRESSERCAM)
find_package(OGMACAM)
find_package(TSCAM)
find_package(STARSHOOTG)
find_package(NNCAM)
find_package(MALLINCAM)
find_package(OMEGONPROCAM)
if (TOUPCAM_FOUND AND ALTAIRCAM_FOUND AND BRESSERCAM_FOUND AND OGMACAM_FOUND AND TSCAM_FOUND AND STARSHOOTG_FOUND AND NNCAM_FOUND AND MALLINCAM_FOUND AND OMEGONPROCAM_FOUND)
add_subdirectory(indi-toupbase)
else (TOUPCAM_FOUND AND ALTAIRCAM_FOUND AND BRESSERCAM_FOUND AND OGMACAM_FOUND AND TSCAM_FOUND AND STARSHOOTG_FOUND AND NNCAM_FOUND AND MALLINCAM_FOUND AND OMEGONPROCAM_FOUND)
if (NOT TOUPCAM_FOUND)
add_subdirectory(libtoupcam)
endif()
if (NOT ALTAIRCAM_FOUND)
add_subdirectory(libaltaircam)
endif()
if (NOT BRESSERCAM_FOUND)
add_subdirectory(libbressercam)
endif()
if (NOT OGMACAM_FOUND)
add_subdirectory(libogmacam)
endif()
if (NOT TSCAM_FOUND)
add_subdirectory(libtscam)
endif()
if (NOT STARSHOOTG_FOUND)
add_subdirectory(libstarshootg)
endif()
if (NOT NNCAM_FOUND)
add_subdirectory(libnncam)
endif()
if (NOT MALLINCAM_FOUND)
add_subdirectory(libmallincam)
endif()
if (NOT OMEGONPROCAM_FOUND)
add_subdirectory(libomegonprocam)
endif()
SET(LIBRARIES_FOUND FALSE)
endif (TOUPCAM_FOUND AND ALTAIRCAM_FOUND AND BRESSERCAM_FOUND AND OGMACAM_FOUND AND TSCAM_FOUND AND STARSHOOTG_FOUND AND NNCAM_FOUND AND MALLINCAM_FOUND AND OMEGONPROCAM_FOUND)
endif (WITH_TOUPBASE)

## Bee Focuser
if (WITH_BEEFOCUS)
add_subdirectory(indi-beefocus)
endif(WITH_BEEFOCUS)


## INOVA
if (WITH_INOVAPLX)
find_package(INOVASDK)
if (INOVASDK_FOUND)
add_subdirectory(indi-inovaplx)
else (INOVASDK_FOUND)
add_subdirectory(libinovasdk)
SET(LIBRARIES_FOUND FALSE)
endif (INOVASDK_FOUND)
endif (WITH_INOVAPLX)

## FLI
if (WITH_FLI)
find_package(FLI)
if (FLI_FOUND)
add_subdirectory(indi-fli)
else (FLI_FOUND)
add_subdirectory(libfli)
SET(LIBRARIES_FOUND FALSE)
endif (FLI_FOUND)
endif(WITH_FLI)

## Apogee
if (WITH_APOGEE)
find_package(APOGEE)
if (APOGEE_FOUND)
add_subdirectory(indi-apogee)
else (APOGEE_FOUND)
add_subdirectory(libapogee)
SET(LIBRARIES_FOUND FALSE)
endif (APOGEE_FOUND)
endif(WITH_APOGEE)

## Point Grey FireFly MV
if (WITH_FFMV)
add_subdirectory(indi-ffmv)
endif (WITH_FFMV)

## Moravian
if (WITH_MI)
find_package(MICAM)
if (MICAM_FOUND)
add_subdirectory(indi-mi)
else (MICAM_FOUND)
add_subdirectory(libmicam)
SET(LIBRARIES_FOUND FALSE)
endif (MICAM_FOUND)
endif (WITH_MI)

## Arduino
if (WITH_DUINO)
add_subdirectory(indi-duino)
endif (WITH_DUINO)

## Fishcamp
if (WITH_FISHCAMP)
find_package(FISHCAMP)
if (FISHCAMP_FOUND)
add_subdirectory(indi-fishcamp)
else (FISHCAMP_FOUND)
add_subdirectory(libfishcamp)
SET(LIBRARIES_FOUND FALSE)
endif(FISHCAMP_FOUND)
endif(WITH_FISHCAMP)

## ASI
if (WITH_ASICAM)
find_package(ASI)
if (ASI_FOUND)
add_subdirectory(indi-asi)
else (ASI_FOUND)
add_subdirectory(libasi)
SET(LIBRARIES_FOUND FALSE)
endif(ASI_FOUND)
endif(WITH_ASICAM)

## DSI
if (WITH_DSI)
add_subdirectory(indi-dsi)
endif(WITH_DSI)

## QHY
if (WITH_QHY)
find_package(QHY)
if (QHY_FOUND)
add_subdirectory(indi-qhy)
else (QHY_FOUND)
add_subdirectory(libqhy)
SET(LIBRARIES_FOUND FALSE)
endif (QHY_FOUND)
endif (WITH_QHY)

## GPSD
if (WITH_GPSD)
add_subdirectory(indi-gpsd)
endif (WITH_GPSD)

## GPS NMEA
if (WITH_GPSNMEA)
add_subdirectory(indi-gpsnmea)
endif(WITH_GPSNMEA)

## RTKLIB
if (WITH_RTKLIB)
add_subdirectory(indi-rtklib)
endif(WITH_RTKLIB)

## GIGE
if (WITH_GIGE)
add_subdirectory(indi-gige)
endif (WITH_GIGE)

# MGen
if (WITH_MGEN)
add_subdirectory(indi-mgen)
endif (WITH_MGEN)

# Astromechanics Focuser
if (WITH_ASTROMECHFOC)
add_subdirectory(indi-astromechfoc)
endif(WITH_ASTROMECHFOC)

## LIME-SDR
if (WITH_LIMESDR)
find_package(LIMESUITE)
if (LIMESUITE_FOUND)
add_subdirectory(indi-limesdr)
else (LIMESUITE_FOUND)
SET(LIBRARIES_FOUND FALSE)
endif (LIMESUITE_FOUND)
endif (WITH_LIMESDR)

if (WITH_ARMADILLO)
add_subdirectory(indi-armadillo-platypus)
endif(WITH_ARMADILLO)

if (WITH_WEBCAM)
add_subdirectory(indi-webcam)
endif()

if (WITH_WEEWX_JSON)
add_subdirectory(indi-weewx-json)
endif()

if (WITH_NIGHTSCAPE)
add_subdirectory(indi-nightscape)
endif(WITH_NIGHTSCAPE)

if (WITH_DREAMFOCUSER)
add_subdirectory(indi-dreamfocuser)
endif()

# Avalon StarGO
if (WITH_AVALON)
add_subdirectory(indi-avalon)
endif(WITH_AVALON)

# Pentax
if (WITH_PENTAX)
find_package(PENTAX)
if (PENTAX_FOUND)
add_subdirectory(indi-pentax)
else (PENTAX_FOUND)
if(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^aarch64")
add_subdirectory(libricohcamerasdk)
endif()
add_subdirectory(libpktriggercord)
SET(LIBRARIES_FOUND FALSE)
endif (PENTAX_FOUND)
endif(WITH_PENTAX)

# AHP
if (WITH_AHP_XC)
find_package(AHPXC)
if(AHP_XC_FOUND)
add_subdirectory(indi-ahp-xc)
else(AHP_XC_FOUND)
add_subdirectory(libahp-xc)
endif(AHP_XC_FOUND)
endif(WITH_AHP_XC)

# svbony
if (WITH_SVBONY)
find_package(SVBONY)
if(SVBONY_FOUND)
add_subdirectory(indi-svbony)
else(SVBONY_FOUND)
add_subdirectory(libsvbony)
SET(LIBRARIES_FOUND FALSE)
endif(SVBONY_FOUND)
endif(WITH_SVBONY)

# rpi_cam
if (WITH_RPICAM)
add_subdirectory(indi-rpicam)
endif(WITH_RPICAM)

# Bresser Exos 2 GoTo
if (WITH_BRESSEREXOS2)
add_subdirectory(indi-bresserexos2)
endif(WITH_BRESSEREXOS2)

# Orion SSG3
if (WITH_ORION_SSG3)
add_subdirectory(indi-orion-ssg3)
endif(WITH_ORION_SSG3)

# PLAYERONE
if (WITH_PLAYERONE)
find_package(PLAYERONE)
if (PLAYERONE_FOUND)
add_subdirectory(indi-playerone)
else (PLAYERONE_FOUND)
add_subdirectory(libplayerone)
SET(LIBRARIES_FOUND FALSE)
endif(PLAYERONE_FOUND)
endif(WITH_PLAYERONE)

# NUT
if (WITH_NUT)
add_subdirectory(indi-nut)
endif()

# Astroasis
if (WITH_ASTROASIS)
find_package(ASTROASIS)
if (ASTROASIS_FOUND)
add_subdirectory(indi-astroasis)
else (ASTROASIS_FOUND)
add_subdirectory(libastroasis)
SET(LIBRARIES_FOUND FALSE)
endif(ASTROASIS_FOUND)
endif(WITH_ASTROASIS)

# Check if libraries are found. If not, we must build them, install them, THEN run CMake again to build and instal the drivers. If all the libraraies are installed, then we build and install the drivers only now.
if (LIBRARIES_FOUND)
message(STATUS "############################################################################")
message(STATUS "######### All libraries are found. Building all INDI 3rd party drivers now.")
message(STATUS "############################################################################")
else (LIBRARIES_FOUND)
message(STATUS "####################################################################################################################################")
message(STATUS "Not all libraries found, you must build and install all libraries first:")

if (WITH_QSI AND NOT QSI_FOUND)
message(STATUS "libqsi was not found and will now be built. Please install libqsi first before running cmake again to install indi-qsi.")
endif (WITH_QSI AND NOT QSI_FOUND)

if (WITH_QHY AND NOT QHY_FOUND)
message(STATUS "libqhy was not found and will now be built. Please install libqhy first before running cmake again to install indi-qhy.")
endif (WITH_QHY AND NOT QHY_FOUND)

if (WITH_SBIG AND NOT SBIG_FOUND)
message(STATUS "libsbigudrv was not found and will now be built. Please install libsbigudrv first before running cmake again to install indi-sbig.")
endif (WITH_SBIG AND NOT SBIG_FOUND)

if (WITH_ATIK AND NOT ATIK_FOUND)
message(STATUS "libatik was not found and will now be built. Please install libatik first before running cmake again to install indi-atik.")
endif (WITH_ATIK AND NOT ATIK_FOUND)

if (WITH_TOUPBASE AND NOT TOUPCAM_FOUND)
message(STATUS "libtoupcam was not found and will now be built. Please install libtoupcam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT TOUPCAM_FOUND)

if (WITH_TOUPBASE AND NOT ALTAIRCAM_FOUND)
message(STATUS "libaltaircam was not found and will now be built. Please install libaltaircam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT ALTAIRCAM_FOUND)

if (WITH_TOUPBASE AND NOT BRESSERCAM_FOUND)
message(STATUS "libbressercam was not found and will now be built. Please install libbressercam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT BRESSERCAM_FOUND)

if (WITH_TOUPBASE AND NOT OGMACAM_FOUND)
message(STATUS "libogmacam was not found and will now be built. Please install libogmacam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT OGMACAM_FOUND)

if (WITH_TOUPBASE AND NOT TSCAM_FOUND)
message(STATUS "libtscam was not found and will now be built. Please install libtscam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT TSCAM_FOUND)

if (WITH_TOUPBASE AND NOT STARSHOOTG_FOUND)
message(STATUS "libstarshootg was not found and will now be built. Please install libstarshootg first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT STARSHOOTG_FOUND)

if (WITH_TOUPBASE AND NOT NNCAM_FOUND)
message(STATUS "libnncam was not found and will now be built. Please install libnncam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT NNCAM_FOUND)

if (WITH_TOUPBASE AND NOT MALLINCAM_FOUND)
message(STATUS "libmallincam was not found and will now be built. Please install libmallincam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT MALLINCAM_FOUND)

if (WITH_TOUPBASE AND NOT OMEGONPROCAM_FOUND)
message(STATUS "libomegonprocam was not found and will now be built. Please install libomegonprocam first before running cmake again to install indi-toupbase.")
endif (WITH_TOUPBASE AND NOT OMEGONPROCAM_FOUND)

if (WITH_INOVAPLX AND NOT INOVASDK_FOUND)
message(STATUS "libinovasdk was not found and will now be built. Please install libinovasdk first before running cmake again to install indi-inovaplx.")
endif (WITH_INOVAPLX AND NOT INOVASDK_FOUND)

if (WITH_FLI AND NOT FLI_FOUND)
message(STATUS "libfli was not found and will now be built. Please install libfli first before running cmake again to install indi-fli.")
endif (WITH_FLI AND NOT FLI_FOUND)

if (WITH_APOGEE AND NOT APOGEE_FOUND)
message(STATUS "libapogee was not found and will now be built. Please install libapogee first before running cmake again to install indi-apogee.")
endif (WITH_APOGEE AND NOT APOGEE_FOUND)

if (WITH_FISHCAMP AND NOT FISHCAMP_FOUND)
message(STATUS "libfishcamp was not found and will now be built. Please install libfishcamp first before running cmake again to install indi-fishcamp.")
endif (WITH_FISHCAMP AND NOT FISHCAMP_FOUND)

if (WITH_LIMESDR AND NOT LIMESDR_FOUND)
message(STATUS "liblimesuite was not found. Please install liblimesuite first before running cmake again to install indi-limesdr.")
endif (WITH_LIMESDR AND NOT LIMESDR_FOUND)

if (WITH_PENTAX AND NOT PENTAX_FOUND)
message(STATUS "libpktriggercord and/or libricohcamerasdk were not found and will now be built. Please install these libraries first before running cmake again to install indi-pentax.")
endif (WITH_PENTAX AND NOT PENTAX_FOUND)

if (WITH_SVBONY AND NOT SVBONY_FOUND)
message(STATUS "libsvbony was not found and will now be built. Please install this libsvbony first before running cmake again to install indi-svbony.")
endif (WITH_SVBONY AND NOT SVBONY_FOUND)

if (WITH_PLAYERONE AND NOT PLAYERONE_FOUND)
message(STATUS "libplayerone was not found and will now be built. Please install this libplayerone first before running cmake again to install indi-playerone.")
endif (WITH_PLAYERONE AND NOT PLAYERONE_FOUND)

if (WITH_AHP_XC AND NOT AHP_XC_FOUND)
message(STATUS "libahp-xc was not found and will now be built. Please install libahp-xc first before running cmake again to install indi-ahp-xc.")
endif (WITH_AHP_XC AND NOT AHP_XC_FOUND)

if (WITH_AHP_GT AND NOT AHP_GT_FOUND)
message(STATUS "libahp-gt was not found and will now be built. Please install libahp-gt first before running cmake again to install indi-ahpgt.")
endif (WITH_AHP_GT AND NOT AHP_GT_FOUND)

if (WITH_ASTROASIS AND NOT ASTROASIS_FOUND)
message(STATUS "libastroasis was not found and will now be built. Please install libastroasis first before running cmake again to install indi-astroasis.")
endif (WITH_ASTROASIS AND NOT ASTROASIS_FOUND)

message(STATUS "####################################################################################################################################")
endif (LIBRARIES_FOUND)

ENDIF(BUILD_LIBS)

