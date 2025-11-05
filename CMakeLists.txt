cmake_minimum_required(VERSION 3.16)

project(libindi-3rdparty CXX C)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND TOUPTEK_REBRANDS TOUPCAM ALTAIRCAM BRESSERCAM MALLINCAM MEADECAM NNCAM OGMACAM OMEGONPROCAM STARSHOOTG TSCAM SVBONYCAM)

include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

## Some files like libnova.h and libusb.h are in in subdirectories of the include directory
## For the CMAKE Modules, they find the subdirectory, so then something like ln_types.h should be #include ln_types.h
## But some packages and drivers write their header files like this: #include libnova/ln_types.h
## On Linux, this is fine since the top include directory such as /usr/include is already included and therefore
## <libnova/ln_types.h> is resolved. But on Mac it its not already in the path and has to be explicitly added.

if(APPLE)
  ##This one is needed for x86_64 homebrew
  include_directories("/usr/local/include")
  ##This one is needed for arm64 homebrew
  include_directories("/opt/homebrew/include")
  ## This one is needed for Craft
  include_directories("${CMAKE_INSTALL_PREFIX}/include")

  # FIX_LIBRARY_LINK Macro
  # This macro will check MacOS Binary Libraries provided by third parties for external links that could cause linking and packaging problems
  # If the FIX_MACOS_LIBS option is enabled when building, it will also attempt to fix the problems, which should then allow the build to continue.
  # This macro should be called by the FIX_MACOS_LIBRARIES macro below
  macro(
    FIX_LIBRARY_LINK
    PACKAGE_NAME
    CURRENT_DYLIB_FILENAME
    LIBRARY_DISPLAY_NAME
    LINK
  )
    if("${LINK}" MATCHES "${PACKAGE_NAME}")
      if(NOT "${LINK}" STREQUAL "@rpath/${PACKAGE_NAME}.dylib")
        if(FIX_MACOS_LIBS)
          execute_process(
            COMMAND
              bash "-c"
              "install_name_tool -change ${LINK} @rpath/${PACKAGE_NAME}.dylib ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME}"
          )
          message(
            WARNING
            "The mac library ${CURRENT_DYLIB_FILENAME} in the ${LIBRARY_DISPLAY_NAME} source folder has a link to ${PACKAGE_NAME}.dylib that says ${LINK}. "
            "The issue is now fixed by changing the link to @rpath/${PACKAGE_NAME}.dylib for better compatibility."
            "Please push the changes to INDI 3rd party repository if you have write access, make a pull request, or let the maintainer know to fix the issue. "
          )
        else(FIX_MACOS_LIBS)
          message(
            FATAL_ERROR
            "The mac library ${CURRENT_DYLIB_FILENAME} in the ${LIBRARY_DISPLAY_NAME} source folder has a link to ${PACKAGE_NAME}.dylib that says ${LINK}. "
            "This could cause issues if ${PACKAGE_NAME}.dylib is not actually at that location. "
            "If you would like to attempt to fix the problem on your machine and continue the build, please enable the FIX_MACOS_LIBS option. "
          )
        endif(FIX_MACOS_LIBS)
      endif(NOT "${LINK}" STREQUAL "@rpath/${PACKAGE_NAME}.dylib")
    endif("${LINK}" MATCHES "${PACKAGE_NAME}")
  endmacro(FIX_LIBRARY_LINK)

  # FIX_LIBRARY_ID Macro
  # This macro will check MacOS Binary Libraries provided by third parties for Install IDs that cold cause linking and packaging problems.
  # If the FIX_MACOS_LIBS option is enabled when building, it will also attempt to fix the problems, which should then allow the build to continue.
  # This macro should be called by the FIX_MACOS_LIBRARIES macro below
  macro(FIX_LIBRARY_ID DYLIB_NAME CURRENT_DYLIB_FILENAME LIBRARY_DISPLAY_NAME)
    execute_process(
      COMMAND
        bash "-c"
        "otool -D ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME} | tail -n 1 | tr -d '\n'"
      OUTPUT_VARIABLE DYLIB_ID
    )
    if(NOT ${DYLIB_ID} STREQUAL "@rpath/${DYLIB_NAME}.dylib")
      if(FIX_MACOS_LIBS)
        execute_process(
          COMMAND
            bash "-c"
            "install_name_tool -id @rpath/${DYLIB_NAME}.dylib ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME}"
        )
        message(
          WARNING
          "The Mac library ${DYLIB_NAME}.dylib in the ${LIBRARY_DISPLAY_NAME} source folder had the ID ${DYLIB_ID} which could cause linking problems. "
          "The issue is now fixed by changing the library ${DYLIB_NAME}.dylib to have the id @rpath/${DYLIB_NAME}.dylib so it links properly with rpaths. "
          "Please push the changes to INDI 3rd party repository if you have write access, make a pull request, or let the maintainer know to fix the issue. "
        )
      else(FIX_MACOS_LIBS)
        message(
          FATAL_ERROR
          "The Mac library ${DYLIB_NAME}.dylib in the ${LIBRARY_DISPLAY_NAME} source folder has the ID ${DYLIB_ID} which could cause linking problems. "
          "Please let the maintainer for ${LIBRARY_DISPLAY_NAME} know about the issue."
          "If you would like to attempt to fix the problem on your machine and continue the build, please enable the FIX_MACOS_LIBS option. "
        )
      endif(FIX_MACOS_LIBS)
    endif(NOT ${DYLIB_ID} STREQUAL "@rpath/${DYLIB_NAME}.dylib")
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

  macro(
    FIX_MACOS_LIBRARIES
    DYLIB_NAME
    CURRENT_DYLIB_FILENAME
    LIBRARY_DISPLAY_NAME
  )
    # This section will use the FIX_LIBRARY_ID macro to check and optionally fix Install IDs that could cause linking issues.
    fix_library_id(${DYLIB_NAME} ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME})

    # This section gets all the current links to other libraries then checks and optionally corrects absolute or bad links to libraries including libusb and opencv
    execute_process(
      COMMAND
        bash "-c"
        "otool -L ${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_DYLIB_FILENAME} | cut -d ' ' -f1"
      OUTPUT_VARIABLE LINKS_LIST
    )
    string(REPLACE "\t" "" LINKS_LIST "${LINKS_LIST}")
    string(REPLACE "\n" ";" LINKS_LIST "${LINKS_LIST}")
    foreach(LINK IN LISTS LINKS_LIST)
      if(NOT ${LINK} STREQUAL "")
        fix_library_link("libusb-1.0.0" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
        fix_library_link("libopencv_highgui" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
        fix_library_link("libopencv_videoio" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
        fix_library_link("libopencv_imgcodecs" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
        fix_library_link("libopencv_imgproc" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
        fix_library_link("libopencv_core" ${CURRENT_DYLIB_FILENAME} ${LIBRARY_DISPLAY_NAME} ${LINK})
      endif(NOT ${LINK} STREQUAL "")
    endforeach(LINK IN LISTS LINKS_LIST)
  endmacro(FIX_MACOS_LIBRARIES)
endif(APPLE)

set(LIBRARIES_FOUND TRUE)

include(CMakeCommon)

# Clang Format support
if(UNIX OR APPLE)
  set(FORMAT_CODE OFF CACHE BOOL "Enable Clang Format")
  if(FORMAT_CODE MATCHES ON)
    file(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

    foreach(SOURCE_FILE ${ALL_SOURCE_FILES})
      string(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)
      if(NOT ${DIR_FOUND} EQUAL 0)
        list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
      endif()
      # Don't apply the format for 3rd party libraries
      string(FIND ${SOURCE_FILE} libapogee DIR1_FOUND)
      string(FIND ${SOURCE_FILE} libfishcamp DIR2_FOUND)
      string(FIND ${SOURCE_FILE} libfli DIR3_FOUND)
      string(FIND ${SOURCE_FILE} libqhy DIR4_FOUND)
      string(FIND ${SOURCE_FILE} libqsi DIR5_FOUND)
      string(FIND ${SOURCE_FILE} libsbig DIR6_FOUND)
      string(FIND ${SOURCE_FILE} libinovasdk DIR8_FOUND)
      string(FIND ${SOURCE_FILE} libsvbony DIR9_FOUND)
      if(
        NOT ${DIR1_FOUND} EQUAL -1
        OR NOT ${DIR2_FOUND} EQUAL -1
        OR NOT ${DIR3_FOUND} EQUAL -1
        OR NOT ${DIR4_FOUND} EQUAL -1
        OR NOT ${DIR5_FOUND} EQUAL -1
        OR NOT ${DIR6_FOUND} EQUAL -1
        OR NOT ${DIR7_FOUND} EQUAL -1
        OR NOT ${DIR8_FOUND} EQUAL -1
        OR NOT ${DIR9_FOUND} EQUAL -1
      )
        list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
      endif()
    endforeach()

    find_program(CLANGFORMAT_EXE NAMES clang-format-5.0)
    if(CLANGFORMAT_EXE)
      add_custom_target(
        clang-format
        COMMAND ${CLANGFORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES}
      )
    endif()
  endif()
endif()

# Option to build the 3rd Party libraries instead of the 3rd Party drivers.
# This is by default OFF, so you must set the option to build them.
# It is a good idea to run with this option before the 3rd Party build so all the libraries are built first.
option(BUILD_LIBS "Build 3rd Party Libraries, not 3rd Party Drivers" Off)

option(INDI_INSTALL_FIRMWARE "Install Firmwares" On)
option(INDI_INSTALL_UDEV_RULES "Install UDEV rules" On)

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
option(WITH_TOUPCAM "Install Toupcam Driver" On)
option(WITH_ALTAIRCAM "Install Altaircam Driver" On)
option(WITH_BRESSERCAM "Install Bressercam Driver" On)
option(WITH_MALLINCAM "Install Mallingcam Driver" On)
option(WITH_MEADECAM "Install Meadecam Driver" On)
option(WITH_NNCAM "Install NNcam Driver" On)
option(WITH_OGMACAM "Install Ogmacam Driver" On)
option(WITH_OMEGONPROCAM "Install Omegonprocam Driver" On)
option(WITH_STARSHOOTG "Install Starshootg Driver" On)
option(WITH_TSCAM "Install TScam Driver" On)
option(WITH_SVBONYCAM "Install Svbonycam Driver" On)
option(WITH_DREAMFOCUSER "Install DreamFocuser Driver" On)
option(WITH_AVALON "Install Avalon StarGO Driver" On)
option(WITH_AVALONUD "Install AvalonUD Drivers" On)
option(WITH_BEEFOCUS "Install Bee Focuser Driver" On)
option(WITH_SHELYAK "Install Shelyak Spectrograph Driver" On)
option(WITH_SKYWALKER "Install AOK SkyWalker Mount Driver" On)
option(WITH_TALON6 "Install Talon6 Mount Driver" On)
option(WITH_PENTAX "Install Pentax Driver" On)
option(WITH_ASTROLINK4 "Install AstroLink4 Driver" On)
option(WITH_AHP_XC "Install AHP XC Correlators Driver" Off)
option(WITH_AHP_GT "Install AHP GT Controllers Driver" Off)
option(WITH_ORION_SSG3 "Install Orion StarShoot G3 Driver" On)
option(WITH_SVBONY "Install SVBONY Camera Driver" On)
option(WITH_BRESSEREXOS2 "Install Bresser Exos 2 GoTo Mount Driver" On)
option(WITH_PLAYERONE "Install Player One Astronomy's Camera Driver" On)
option(WITH_WEEWX_JSON "Install Weewx JSON Driver" On)
option(WITH_ROLLOFFINO "Install RollOff ino Dome Driver" On)
option(WITH_ASTROASIS "Install Astroasis Driver" On)
option(WITH_OCS "Install OCS Driver" On)
option(WITH_GPIO "Install GPIO Driver" On)
option(WITH_TICFOCUSER-NG "Install TICFOCUSER-NG Driver" On)
option(WITH_OPENOGMA "Install OpenOGMA Driver" On)
option(WITH_LIBCAMERA "Install Libcamera Driver (Raspberry PI)" Off)
option(WITH_BNO_IMU "Install BNO IMU Driver" Off)
option(WITH_CELESTRON_ORIGIN "Install Celestron Origin Driver" Off)

# FFMPEG required for INDI Webcam driver
find_package(FFmpeg)
if(FFMPEG_FOUND)
  message(STATUS "Since FFMPEG was found, INDI Webcam Driver can be built")
  option(WITH_WEBCAM "Install INDI Webcam Driver based on FFMPEG" On)
else(FFMPEG_FOUND)
  message(
    STATUS
    "Since an up to date FFMPEG was not found, INDI Webcam Driver will not be built"
  )
  option(WITH_WEBCAM "Install INDI Webcam Driver based on FFMPEG" Off)
endif(FFMPEG_FOUND)

# MMAL Required for Raspberry PI camera driver
if(
  CMAKE_SYSTEM_PROCESSOR MATCHES "armv+"
  OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64"
)
  find_package(MMAL)
endif()

find_package(NUTClient)
if(NUTCLIENT_FOUND)
  message(STATUS "Since nutclient was found, INDI NUT Driver can be built")
  option(WITH_NUT "Install INDI NUT Driver" On)
else(NUTCLIENT_FOUND)
  message(
    STATUS
    "Since an up to date nutclient was not found, INDI NUT Driver will not be built"
  )
  option(WITH_NUT "Install INDI NUT Driver" Off)
endif(NUTCLIENT_FOUND)

# Add/remove cases for OSX
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(WITH_INOVAPLX Off)
  set(WITH_GIGE Off)
  set(WITH_PENTAX Off)
  set(WITH_GPSD Off)
  set(WITH_AHP_XC Off)
  set(WITH_AHP_GT Off)
  set(WITH_TICFOCUSER-NG Off)
  # The drivers below are not yet compatible with Apple Silicon since their libraries are not universal binaries
  if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(WITH_ASICAM Off)
    set(WITH_SBIG Off)
    set(WITH_ATIK Off)
    set(WITH_TOUPCAM Off)
    set(WITH_ALTAIRCAM Off)
    set(WITH_BRESSERCAM Off)
    set(WITH_MALLINCAM Off)
    set(WITH_MEADECAM Off)
    set(WITH_NNCAM Off)
    set(WITH_OGMACAM Off)
    set(WITH_OMEGONPROCAM Off)
    set(WITH_STARSHOOTG Off)
    set(WITH_TSCAM Off)
    set(WITH_SVBONYCAM Off)
    set(WITH_QHY Off)
    set(WITH_SVBONY Off)
    set(WITH_ASTROASIS Off)
  endif()
endif()
# Disable apogee, qhy and mi with gcc 4.8 and earlier versions
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
  set(WITH_APOGEE Off)
  set(WITH_QHY Off)
  set(WITH_MI Off)
endif()

# Disable pre-built SDKs / drivers
option(NO_PRE_BUILT "Do not install pre-built libraries/drivers" Off)
if(NO_PRE_BUILT)
  set(WITH_AHP_GT Off)
  set(WITH_AHP_XC Off)
  set(WITH_ASICAM Off)
  set(WITH_ATIK Off)
  set(WITH_DSI Off)
  set(WITH_DUINO Off)
  set(WITH_FISHCAMP Off)
  set(WITH_FLI Off)
  set(WITH_INOVAPLX Off)
  set(WITH_MI Off)
  set(WITH_PENTAX Off)
  set(WITH_PLAYERONE Off)
  set(WITH_QHY Off)
  set(WITH_QSI Off)
  set(WITH_SBIG Off)
  set(WITH_SVBONY Off)
  set(WITH_TOUPCAM Off)
  set(WITH_ALTAIRCAM Off)
  set(WITH_BRESSERCAM Off)
  set(WITH_MALLINCAM Off)
  set(WITH_MEADECAM Off)
  set(WITH_NNCAM Off)
  set(WITH_OGMACAM Off)
  set(WITH_OMEGONPROCAM Off)
  set(WITH_STARSHOOTG Off)
  set(WITH_TSCAM Off)
  set(WITH_SVBONYCAM Off)
  set(WITH_ASTROASIS Off)
  SET(WITH_OPENOGMA Off)
endif(NO_PRE_BUILT)

# If the Build Libs option is selected, it will just build the required libraries.
# This should be run before the main 3rd Party Drivers build, so the drivers can find the libraries.
if(BUILD_LIBS)
  #libapogee
  if(WITH_APOGEE)
    add_subdirectory(libapogee)
  endif(WITH_APOGEE)

  #libasi
  if(WITH_ASICAM)
    add_subdirectory(libasi)
  endif(WITH_ASICAM)

  #libatik
  if(WITH_ATIK)
    add_subdirectory(libatik)
  endif(WITH_ATIK)

  #libfishcamp
  if(WITH_FISHCAMP)
    add_subdirectory(libfishcamp)
  endif(WITH_FISHCAMP)

  #libfli
  if(WITH_FLI)
    add_subdirectory(libfli)
  endif(WITH_FLI)

  #libmicam
  if(WITH_MI)
    add_subdirectory(libmicam)
  endif(WITH_MI)

  #libinovasdk
  if(WITH_INOVAPLX)
    add_subdirectory(libinovasdk)
  endif(WITH_INOVAPLX)

  #libqhy
  if(WITH_QHY)
    add_subdirectory(libqhy)
  endif(WITH_QHY)

  #libqsi
  if(WITH_QSI)
    add_subdirectory(libqsi)
  endif(WITH_QSI)

  #libsbig
  if(WITH_SBIG)
    add_subdirectory(libsbig)
  endif(WITH_SBIG)

  #libpentax
  if(WITH_PENTAX)
    if(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^aarch64")
      add_subdirectory(libricohcamerasdk)
    endif()
    add_subdirectory(libpktriggercord)
  endif(WITH_PENTAX)

  #toupbase dependencies
  foreach(BRAND IN LISTS TOUPTEK_REBRANDS)
    set(BRAND_CONDITIONAL "WITH_${BRAND}")
    string(TOLOWER ${BRAND} LIBNAME)
    set(BRAND_LIB "lib${LIBNAME}")

    if(${BRAND_CONDITIONAL})
      add_subdirectory(${BRAND_LIB})
    endif()
  endforeach()

  #libsvbiny
  if(WITH_SVBONY)
    add_subdirectory(libsvbony)
  endif(WITH_SVBONY)

  #libplayerone
  if(WITH_PLAYERONE)
    add_subdirectory(libplayerone)
  endif(WITH_PLAYERONE)

  #libastroasis
  if(WITH_ASTROASIS)
    add_subdirectory(libastroasis)
  endif(WITH_ASTROASIS)

  # This is the main 3rd Party build.  It runs if the Build Libs option is not selected.
else(BUILD_LIBS)
  ## TicFocuser-ng
  if(WITH_TICFOCUSER-NG)
    add_subdirectory(indi-ticfocuser-ng)
  endif(WITH_TICFOCUSER-NG)

  ## EQMod
  if(WITH_EQMOD)
    add_subdirectory(indi-eqmod)
  endif(WITH_EQMOD)

  ## SkyWalker
  if(WITH_SKYWALKER)
    add_subdirectory(indi-aok)
  endif(WITH_SKYWALKER)

  ## Starbook
  if(WITH_STARBOOK)
    add_subdirectory(indi-starbook)
  endif(WITH_STARBOOK)

  ## Starbook Ten
  if(WITH_STARBOOK_TEN)
    add_subdirectory(indi-starbook-ten)
  endif(WITH_STARBOOK_TEN)

  ## Stalight Xpress
  if(WITH_SX)
    add_subdirectory(indi-sx)
  endif(WITH_SX)

  ## Maxdome II
  if(WITH_MAXDOME)
    add_subdirectory(indi-maxdomeii)
  endif(WITH_MAXDOME)

  ## NexDome
  if(WITH_NEXDOME)
    add_subdirectory(indi-nexdome)
  endif(WITH_NEXDOME)

  ## Talon6
  if(WITH_TALON6)
    add_subdirectory(indi-talon6)
  endif(WITH_TALON6)

  ## Rolloffino
  if(WITH_ROLLOFFINO)
    add_subdirectory(indi-rolloffino)
  endif(WITH_ROLLOFFINO)

  ## Shelyak
  if(WITH_SHELYAK)
    add_subdirectory(indi-shelyak)
  endif(WITH_SHELYAK)

  ## Cloud Watcher
  if(WITH_CLOUDWATCHER)
    add_subdirectory(indi-aagcloudwatcher-ng)
  endif(WITH_CLOUDWATCHER)

  ## Celestron AUX
  if(WITH_CAUX)
    add_subdirectory(indi-celestronaux)
  endif(WITH_CAUX)

  ## GPhoto
  if(WITH_GPHOTO)
    add_subdirectory(indi-gphoto)
  endif(WITH_GPHOTO)

  ## QSI
  if(WITH_QSI)
    find_package(QSI)
    if(QSI_FOUND)
      add_subdirectory(indi-qsi)
    else(QSI_FOUND)
      add_subdirectory(libqsi)
      set(LIBRARIES_FOUND FALSE)
    endif(QSI_FOUND)
  endif(WITH_QSI)

  ## SBIG
  if(WITH_SBIG)
    find_package(SBIG)
    if(SBIG_FOUND)
      add_subdirectory(indi-sbig)
    else(SBIG_FOUND)
      add_subdirectory(libsbig)
      set(LIBRARIES_FOUND FALSE)
    endif(SBIG_FOUND)
  endif(WITH_SBIG)

  ## ATIK
  if(WITH_ATIK)
    find_package(ATIK)
    if(ATIK_FOUND)
      add_subdirectory(indi-atik)
    else(ATIK_FOUND)
      add_subdirectory(libatik)
      set(LIBRARIES_FOUND FALSE)
    endif(ATIK_FOUND)
  endif(WITH_ATIK)

  set(ADD_TOUPBASE FALSE)
  ## TOUPBASE driver pack
  foreach(BRAND IN LISTS TOUPTEK_REBRANDS)
    set(BRAND_CONDITIONAL "WITH_${BRAND}")
    set(BRAND_LIB_FOUND "${BRAND}_FOUND")
    string(TOLOWER ${BRAND} LIBNAME)
    set(BRAND_LIB "lib${LIBNAME}")

    if(${BRAND_CONDITIONAL})
      find_package(${BRAND})

      if(${BRAND_LIB_FOUND})
        set(ADD_TOUPBASE TRUE)
      else()
        add_subdirectory(${BRAND_LIB})
        set(LIBRARIES_FOUND FALSE)
      endif()
    endif()
  endforeach()

  if(ADD_TOUPBASE AND LIBRARIES_FOUND)
    add_subdirectory(indi-toupbase)
  endif(ADD_TOUPBASE AND LIBRARIES_FOUND)

  ## Bee Focuser
  if(WITH_BEEFOCUS)
    add_subdirectory(indi-beefocus)
  endif(WITH_BEEFOCUS)

  ## INOVA
  if(WITH_INOVAPLX)
    find_package(INOVASDK)
    if(INOVASDK_FOUND)
      add_subdirectory(indi-inovaplx)
    else(INOVASDK_FOUND)
      add_subdirectory(libinovasdk)
      set(LIBRARIES_FOUND FALSE)
    endif(INOVASDK_FOUND)
  endif(WITH_INOVAPLX)

  ## FLI
  if(WITH_FLI)
    find_package(FLI)
    if(FLI_FOUND)
      add_subdirectory(indi-fli)
    else(FLI_FOUND)
      add_subdirectory(libfli)
      set(LIBRARIES_FOUND FALSE)
    endif(FLI_FOUND)
  endif(WITH_FLI)

  ## Apogee
  if(WITH_APOGEE)
    find_package(APOGEE)
    if(APOGEE_FOUND)
      add_subdirectory(indi-apogee)
    else(APOGEE_FOUND)
      add_subdirectory(libapogee)
      set(LIBRARIES_FOUND FALSE)
    endif(APOGEE_FOUND)
  endif(WITH_APOGEE)

  ## Point Grey FireFly MV
  if(WITH_FFMV)
    add_subdirectory(indi-ffmv)
  endif(WITH_FFMV)

  ## Moravian
  if(WITH_MI)
    find_package(MICAM)
    if(MICAM_FOUND)
      add_subdirectory(indi-mi)
    else(MICAM_FOUND)
      add_subdirectory(libmicam)
      set(LIBRARIES_FOUND FALSE)
    endif(MICAM_FOUND)
  endif(WITH_MI)

  ## Arduino
  if(WITH_DUINO)
    add_subdirectory(indi-duino)
  endif(WITH_DUINO)

  ## Fishcamp
  if(WITH_FISHCAMP)
    find_package(FISHCAMP)
    if(FISHCAMP_FOUND)
      add_subdirectory(indi-fishcamp)
    else(FISHCAMP_FOUND)
      add_subdirectory(libfishcamp)
      set(LIBRARIES_FOUND FALSE)
    endif(FISHCAMP_FOUND)
  endif(WITH_FISHCAMP)

  ## ASI
  if(WITH_ASICAM)
    find_package(ASI)
    if(ASI_FOUND)
      add_subdirectory(indi-asi)
    else(ASI_FOUND)
      add_subdirectory(libasi)
      set(LIBRARIES_FOUND FALSE)
    endif(ASI_FOUND)
  endif(WITH_ASICAM)

  ## DSI
  if(WITH_DSI)
    add_subdirectory(indi-dsi)
  endif(WITH_DSI)

  ## QHY
  if(WITH_QHY)
    find_package(QHY)
    if(QHY_FOUND)
      add_subdirectory(indi-qhy)
    else(QHY_FOUND)
      add_subdirectory(libqhy)
      set(LIBRARIES_FOUND FALSE)
    endif(QHY_FOUND)
  endif(WITH_QHY)

  ## GPSD
  if(WITH_GPSD)
    add_subdirectory(indi-gpsd)
  endif(WITH_GPSD)

  ## GPS NMEA
  if(WITH_GPSNMEA)
    add_subdirectory(indi-gpsnmea)
  endif(WITH_GPSNMEA)

  ## RTKLIB
  if(WITH_RTKLIB)
    add_subdirectory(indi-rtklib)
  endif(WITH_RTKLIB)

  ## GIGE
  if(WITH_GIGE)
    add_subdirectory(indi-gige)
  endif(WITH_GIGE)

  # MGen
  if(WITH_MGEN)
    add_subdirectory(indi-mgen)
  endif(WITH_MGEN)

  ## LIME-SDR
  if(WITH_LIMESDR)
    find_package(LIMESUITE)
    if(LIMESUITE_FOUND)
      add_subdirectory(indi-limesdr)
    else(LIMESUITE_FOUND)
      set(LIBRARIES_FOUND FALSE)
    endif(LIMESUITE_FOUND)
  endif(WITH_LIMESDR)

  if(WITH_ARMADILLO)
    add_subdirectory(indi-armadillo-platypus)
  endif(WITH_ARMADILLO)

  if(WITH_WEBCAM)
    add_subdirectory(indi-webcam)
  endif()

  if(WITH_WEEWX_JSON)
    add_subdirectory(indi-weewx-json)
  endif()

  if(WITH_NIGHTSCAPE)
    add_subdirectory(indi-nightscape)
  endif(WITH_NIGHTSCAPE)

  # Avalon StarGO
  if(WITH_AVALON)
    add_subdirectory(indi-avalon)
  endif(WITH_AVALON)

  # AvalonUD
  if(WITH_AVALONUD)
    add_subdirectory(indi-avalonud)
  endif(WITH_AVALONUD)

  # Pentax
  if(WITH_PENTAX)
    find_package(PENTAX)
    if(PENTAX_FOUND)
      add_subdirectory(indi-pentax)
    else(PENTAX_FOUND)
      if(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^aarch64")
        add_subdirectory(libricohcamerasdk)
      endif()
      add_subdirectory(libpktriggercord)
      set(LIBRARIES_FOUND FALSE)
    endif(PENTAX_FOUND)
  endif(WITH_PENTAX)

  # AHP
  if(WITH_AHP_XC)
    find_package(AHPXC)
    if(AHP_XC_FOUND)
      add_subdirectory(indi-ahp-xc)
    endif(AHP_XC_FOUND)
  endif(WITH_AHP_XC)

  # svbony
  if(WITH_SVBONY)
    find_package(SVBONY)
    if(SVBONY_FOUND)
      add_subdirectory(indi-svbony)
    else(SVBONY_FOUND)
      add_subdirectory(libsvbony)
      set(LIBRARIES_FOUND FALSE)
    endif(SVBONY_FOUND)
  endif(WITH_SVBONY)

  # Bresser Exos 2 GoTo
  if(WITH_BRESSEREXOS2)
    add_subdirectory(indi-bresserexos2)
  endif(WITH_BRESSEREXOS2)

  # Orion SSG3
  if(WITH_ORION_SSG3)
    add_subdirectory(indi-orion-ssg3)
  endif(WITH_ORION_SSG3)

  # PLAYERONE
  if(WITH_PLAYERONE)
    find_package(PLAYERONE)
    if(PLAYERONE_FOUND)
      add_subdirectory(indi-playerone)
    else(PLAYERONE_FOUND)
      add_subdirectory(libplayerone)
      set(LIBRARIES_FOUND FALSE)
    endif(PLAYERONE_FOUND)
  endif(WITH_PLAYERONE)

  # NUT
  if(WITH_NUT)
    add_subdirectory(indi-nut)
  endif()

  # Astroasis
  if(WITH_ASTROASIS)
    find_package(ASTROASIS)
    if(ASTROASIS_FOUND)
      add_subdirectory(indi-astroasis)
    else(ASTROASIS_FOUND)
      add_subdirectory(libastroasis)
      set(LIBRARIES_FOUND FALSE)
    endif(ASTROASIS_FOUND)
  endif(WITH_ASTROASIS)

  # OCS
  if(WITH_OCS)
    add_subdirectory(indi-ocs)
  endif()

  # GPIO
  if(WITH_GPIO)
    find_package(GPIOD)
    if(GPIOD_FOUND)
      add_subdirectory(indi-gpio)
    endif(GPIOD_FOUND)
  endif()

  # OpenOGMA
  if (WITH_OPENOGMA)
    add_subdirectory(indi-openogma)
  endif(WITH_OPENOGMA)

  # LIBCAMERA
  if (WITH_LIBCAMERA)
    add_subdirectory(indi-libcamera)
  endif(WITH_LIBCAMERA)

  # BNO_IMU
  if (WITH_BNO_IMU)
    add_subdirectory(indi-bno-imu)
  endif(WITH_BNO_IMU)

  # CELESTRON ORIGIN
  if (WITH_CELESTRON_ORIGIN)
    add_subdirectory(indi-celestron-origin)
  endif(WITH_CELESTRON_ORIGIN)

  # Check if libraries are found. If not, we must build them, install them, THEN run CMake again to build and instal the drivers. If all the libraraies are installed, then we build and install the drivers only now.
  if(LIBRARIES_FOUND)
    message(
      STATUS
      "############################################################################"
    )
    message(
      STATUS
      "######### All libraries are found. Building all INDI 3rd party drivers now."
    )
    message(
      STATUS
      "############################################################################"
    )
  else(LIBRARIES_FOUND)
    message(
      STATUS
      "####################################################################################################################################"
    )
    message(
      STATUS
      "Not all libraries found, you must build and install all libraries first:"
    )

    if(WITH_QSI AND NOT QSI_FOUND)
      message(
        STATUS
        "libqsi was not found and will now be built. Please install libqsi first before running cmake again to install indi-qsi."
      )
    endif(WITH_QSI AND NOT QSI_FOUND)

    if(WITH_QHY AND NOT QHY_FOUND)
      message(
        STATUS
        "libqhy was not found and will now be built. Please install libqhy first before running cmake again to install indi-qhy."
      )
    endif(WITH_QHY AND NOT QHY_FOUND)

    if(WITH_SBIG AND NOT SBIG_FOUND)
      message(
        STATUS
        "libsbigudrv was not found and will now be built. Please install libsbigudrv first before running cmake again to install indi-sbig."
      )
    endif(WITH_SBIG AND NOT SBIG_FOUND)

    if(WITH_ATIK AND NOT ATIK_FOUND)
      message(
        STATUS
        "libatik was not found and will now be built. Please install libatik first before running cmake again to install indi-atik."
      )
    endif(WITH_ATIK AND NOT ATIK_FOUND)

    foreach(BRAND IN LISTS TOUPTEK_REBRANDS)
      set(BRAND_CONDITIONAL "WITH_${BRAND}")
      set(BRAND_LIB_FOUND "${BRAND}_FOUND")
      string(TOLOWER ${BRAND} LIBNAME)
      set(BRAND_LIB "lib${LIBNAME}")

      if(${BRAND_CONDITIONAL} AND NOT ${BRAND_LIB_FOUND})
        message(
          STATUS
          "${BRAND_LIB} was not found and will now be built. Please install ${BRAND_LIB} first before running cmake again to install indi-${LIBNAME}."
        )
      endif()
    endforeach()

    if(WITH_INOVAPLX AND NOT INOVASDK_FOUND)
      message(
        STATUS
        "libinovasdk was not found and will now be built. Please install libinovasdk first before running cmake again to install indi-inovaplx."
      )
    endif(WITH_INOVAPLX AND NOT INOVASDK_FOUND)

    if(WITH_FLI AND NOT FLI_FOUND)
      message(
        STATUS
        "libfli was not found and will now be built. Please install libfli first before running cmake again to install indi-fli."
      )
    endif(WITH_FLI AND NOT FLI_FOUND)

    if(WITH_APOGEE AND NOT APOGEE_FOUND)
      message(
        STATUS
        "libapogee was not found and will now be built. Please install libapogee first before running cmake again to install indi-apogee."
      )
    endif(WITH_APOGEE AND NOT APOGEE_FOUND)

    if(WITH_FISHCAMP AND NOT FISHCAMP_FOUND)
      message(
        STATUS
        "libfishcamp was not found and will now be built. Please install libfishcamp first before running cmake again to install indi-fishcamp."
      )
    endif(WITH_FISHCAMP AND NOT FISHCAMP_FOUND)

    if(WITH_LIMESDR AND NOT LIMESDR_FOUND)
      message(
        STATUS
        "liblimesuite was not found. Please install liblimesuite first before running cmake again to install indi-limesdr."
      )
    endif(WITH_LIMESDR AND NOT LIMESDR_FOUND)

    if(WITH_PENTAX AND NOT PENTAX_FOUND)
      message(
        STATUS
        "libpktriggercord and/or libricohcamerasdk were not found and will now be built. Please install these libraries first before running cmake again to install indi-pentax."
      )
    endif(WITH_PENTAX AND NOT PENTAX_FOUND)

    if(WITH_SVBONY AND NOT SVBONY_FOUND)
      message(
        STATUS
        "libsvbony was not found and will now be built. Please install this libsvbony first before running cmake again to install indi-svbony."
      )
    endif(WITH_SVBONY AND NOT SVBONY_FOUND)

    if(WITH_PLAYERONE AND NOT PLAYERONE_FOUND)
      message(
        STATUS
        "libplayerone was not found and will now be built. Please install this libplayerone first before running cmake again to install indi-playerone."
      )
    endif(WITH_PLAYERONE AND NOT PLAYERONE_FOUND)

    if(WITH_ASTROASIS AND NOT ASTROASIS_FOUND)
      message(
        STATUS
        "libastroasis was not found and will now be built. Please install libastroasis first before running cmake again to install indi-astroasis."
      )
    endif(WITH_ASTROASIS AND NOT ASTROASIS_FOUND)

    message(
      STATUS
      "####################################################################################################################################"
    )
  endif(LIBRARIES_FOUND)
endif(BUILD_LIBS)
