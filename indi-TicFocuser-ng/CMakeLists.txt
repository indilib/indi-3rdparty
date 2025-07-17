if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

PROJECT(TicFocuserNG CXX)

set (TICFOCUSER_VERSION_MAJOR 1)
set (TICFOCUSER_VERSION_MINOR 1)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(CMakeCommon)

# Définition des sources de l'exécutable principal
# Ces sources seront ajoutées à la cible finale après que toutes les dépendances soient gérées.
set(INDI_TICFOCUSER-NG_CORE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/UsbConnectionBase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/SerialConnection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/TiclibInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicBase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicDefs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/StreamSerial.cpp
    )

add_executable(indi_ticfocuser-ng ${INDI_TICFOCUSER-NG_CORE_SRCS}) # Ajoute les sources dès maintenant

# --- Chemins d'inclusion généraux ---
# CMAKE_CURRENT_BINARY_DIR doit être le premier pour les headers générés
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}) # Pour les headers du projet TicFocuserNG
# include_directories( ${INDI_INCLUDE_DIR} ) # INDI_INCLUDE_DIR sera géré par target_link_libraries avec INDI

find_package(INDI REQUIRED)
target_link_libraries( indi_ticfocuser-ng ${INDI_LIBRARIES}) # INDI_INCLUDE_DIR est ajouté par INDI_LIBRARIES/target_link_libraries

# --- Add include directories for your main target ---
target_include_directories(indi_ticfocuser-ng PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}              # For your project's own headers (e.g., TicFocuser.h)
    ${CMAKE_CURRENT_BINARY_DIR}              # For generated headers (e.g., TicFocuser_config.h)
    ${INDI_INCLUDE_DIR}                      # <--- Add this for INDI headers
)

# --- REVISED: Use pkg-config for Libusb-1.0 ---
# First, ensure PkgConfig is found, though you already have this earlier.
# find_package(PkgConfig) # This should already be in CMakeCommon or elsewhere, but good to ensure.

pkg_check_modules(LIBUSB1_PC libusb-1.0 REQUIRED) # Finds libusb-1.0 using pkg-config
message(STATUS "Found Libusb-1.0 via pkg-config: Includes=${LIBUSB1_PC_INCLUDE_DIRS} Libraries=${LIBUSB1_PC_LIBRARIES}")

# Add Libusb-1.0 include directories and link libraries
target_include_directories(indi_ticfocuser-ng PUBLIC ${LIBUSB1_PC_INCLUDE_DIRS})
target_link_libraries(indi_ticfocuser-ng ${LIBUSB1_PC_LIBRARIES})

# --- Détection et Fallback pour LIBUSBP-1 (libusbp) ---
pkg_check_modules(LIBUSBP_PC libusbp) # Le nom du package pkg-config est 'libusbp'

if (NOT LIBUSBP_PC_FOUND)
    message(STATUS "LIBUSBP-1 (libusbp) not found on system via pkg-config. Trying to build from bundled libusbp.")

    set(LIBUSBP_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusbp")
    if(EXISTS "${LIBUSBP_ROOT_DIR}/CMakeLists.txt") # Vérifie l'existence du CMakeLists.txt racine de libusbp
        message(STATUS "Found bundled libusbp at '${LIBUSBP_ROOT_DIR}'.")

        # Ajoute le sous-répertoire RACINE du projet libusbp.
        # Cela va exécuter le CMakeLists.txt de libusbp qui, à son tour, appellera add_subdirectory(src)
        add_subdirectory("${LIBUSBP_ROOT_DIR}" libusbp_build_dir) # Renommé pour plus de clarté dans le dossier de build

        # Vérifie si la cible 'usbp' a été créée par le sous-projet
        if(TARGET usbp)
            set(LIBUSBP_PC_LIBRARIES usbp) # La cible de la bibliothèque est bien 'usbp'
            # Le dossier d'inclusion de libusbp est ${LIBUSBP_ROOT_DIR}/include
            set(LIBUSBP_PC_INCLUDE_DIRS "${LIBUSBP_ROOT_DIR}/include")
            set(LIBUSBP_PC_FOUND TRUE) # Marque comme trouvé
            message(STATUS "Successfully configured using bundled libusbp library 'usbp'.")
        else()
            message(WARNING "Bundled libusbp found, but 'usbp' target not created. Please check its CMakeLists.txt and libusbp/src/CMakeLists.txt.")
        endif()
    else()
        message(WARNING "Bundled libusbp not found at '${LIBUSBP_ROOT_DIR}' or it does not contain a CMakeLists.txt.")
    endif()
endif()

# Ce bloc est exécuté si LIBUSBP_PC_FOUND est TRUE (soit par pkg_check_modules, soit par notre fallback)
if (LIBUSBP_PC_FOUND)
    message(STATUS "Using LIBUSBP-1 (libusbp) (from system or bundled source).")
    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/LibUsbConnection.cpp # Supposons que ceci dépend de libusbp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicUsb.cpp # Ceci aussi
    )

    include_directories(${LIBUSBP_PC_INCLUDE_DIRS})
    target_link_libraries(indi_ticfocuser-ng "${LIBUSBP_PC_LIBRARIES}")

    set(WITH_LIBUSBP TRUE)
else()
    message(WARNING "LIBUSBP-1 (libusbp) was not found on the system and could not be built from bundled source. USB functionality might be limited.")
endif()


# --- Traitement de l'autre find_package(USB1) si il existe toujours et est distinct ---
# Note : Si 'USB1' est en fait la même chose que 'libusbp', ce bloc est redondant et pourrait causer des conflits.
# Si USB1 est une autre lib USB (e.g., libusb-0.1 ou autre), alors c'est OK.
# Si tu ne veux qu'une seule lib USB (libusbp), tu peux supprimer ce bloc.
find_package(USB1) # C'est pour l'autre librairie, selon tes dires
if (USB1_FOUND AND NOT WITH_LIBUSBP) # Ajout de NOT WITH_LIBUSBP pour éviter les doublons si USB1 est une alternative
    message(STATUS "Using other USB1 library found on system.")
    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/LibUsbConnection.cpp # Potentiellement dupliqué avec le bloc LIBUSBP_PC si c'est la même fonctionnalité
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/TicUsb.cpp # Potentiellement dupliqué
    )
    include_directories(${USB1_INCLUDE_DIRS})
    target_link_libraries( indi_ticfocuser-ng ${USB1_LIBRARY})
    set(WITH_LIBUSB TRUE)
elseif(USB1_FOUND AND WITH_LIBUSBP)
    message(STATUS "USB1 found, but LIBUSBP-1 already configured. Skipping USB1 to avoid conflicts.")
endif()


find_package(LIBBLUETOOTH)
if (LIBBLUETOOTH_FOUND)
    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/ticlib/StreamBT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/BluetoothConnection.cpp
    )
    target_link_libraries( indi_ticfocuser-ng "${LIBBLUETOOTH_LIBRARIES}")
    set(WITH_BLUETOOTH TRUE)
endif()


# --- Détection et Fallback pour LIBTIC-1 (libpololu-tic-1) ---
# Ce bloc doit venir APRÈS le bloc LIBUSBP-1 car pololu-tic-software en dépend.
pkg_check_modules(LIBTIC-1 libpololu-tic-1)

if(NOT LIBTIC-1_FOUND)
    message(STATUS "LIBTIC-1 (libpololu-tic-1) not found on system. Trying to build from bundled pololu-tic-software.")

    set(POLOLU_TIC_SOFTWARE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pololu-tic-software")
    if(EXISTS "${POLOLU_TIC_SOFTWARE_ROOT_DIR}/CMakeLists.txt")
        message(STATUS "Found bundled pololu-tic-software at '${POLOLU_TIC_SOFTWARE_ROOT_DIR}'.")

        # Passe les informations de libusbp au sous-projet si elles sont définies et si le sous-projet
        # a besoin de savoir comment lier libusbp (sinon il tentera de le trouver lui-même, ce qui cause des erreurs)
        # Ces variables doivent correspondre à ce que pololu-tic-software/lib/CMakeLists.txt attend s'il y a une option.
        # Si pololu-tic-software/lib/CMakeLists.txt utilise simplement pkg_check_modules(LIBUSBP libusbp-1 QUIET),
        # il trouvera la libusbp compilée par le parent (si la cible 'usbp' est globale ou les chemins sont bons).
        # Normalement, en faisant add_subdirectory(libusbp), la cible 'usbp' devient visible globalement.

        add_subdirectory("${POLOLU_TIC_SOFTWARE_ROOT_DIR}" pololu_tic_software_build_dir) # Renommé pour clarté

        # Vérifie si la cible 'lib' (nom interne de la lib Pololu Tic) a été créée.
        if(TARGET lib) # Cible interne trouvée dans pololu-tic-software/lib/CMakeLists.txt est 'lib'
            set(LIBTIC-1_LIBRARIES lib) # La cible de la bibliothèque est 'lib'
            # Le dossier d'inclusion est ${POLOLU_TIC_SOFTWARE_ROOT_DIR}/include
            set(LIBTIC-1_INCLUDE_DIR "${POLOLU_TIC_SOFTWARE_ROOT_DIR}/include")
            set(LIBTIC-1_FOUND TRUE) # Marque comme trouvé
            message(STATUS "Successfully configured using bundled pololu-tic-software library 'lib'.")
        else()
            message(WARNING "Bundled pololu-tic-software found, but 'lib' target not created. Please check its CMakeLists.txt and pololu-tic-software/lib/CMakeLists.txt.")
        endif()
    else()
        message(WARNING "Bundled pololu-tic-software not found at '${POLOLU_TIC_SOFTWARE_ROOT_DIR}' or it does not contain a CMakeLists.txt.")
    endif()
endif()

if(LIBTIC-1_FOUND)
    message(STATUS "Using LIBTIC-1 (from system or bundled source).")
    list(APPEND INDI_TICFOCUSER-NG_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/driver_interfaces/PololuUsbInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection/PololuUsbConnection.cpp
    )

    if(LIBTIC-1_INCLUDE_DIR)
        include_directories(${LIBTIC-1_INCLUDE_DIR})
    endif()

    # CFLAGS doit être appliqué aux sources AVANT la compilation
    # C'est généralement mieux géré par les targets elles-mêmes ou via PUBLIC/PRIVATE sur les libs liées
    # Plutôt que de les passer comme ça si elles ne viennent pas du sous-projet.
    # Si LIBTIC-1_CFLAGS est défini par pkg_check_modules, utilise-le.
    #target_compile_options(indi_ticfocuser-ng PUBLIC ${LIBTIC-1_CFLAGS}) # Commenté car peut être source de problèmes.

    target_link_libraries(indi_ticfocuser-ng "${LIBTIC-1_LIBRARIES}")

    set(WITH_LIBTIC TRUE)
else()
    message(WARNING "LIBTIC-1 (libpololu-tic-1) was not found on the system and could not be built from bundled source. Functionality might be limited.")
endif()

target_sources(indi_ticfocuser-ng PUBLIC ${INDI_TICFOCUSER-NG_SRCS}) # Assure-toi que toutes les sources sont ajoutées.
# Note: Les sources spécifiques aux libs sont ajoutées via list(APPEND ...) dans les blocs IF.


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/TicFocuser_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/TicFocuser_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_ticfocuser-ng.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser-ng.xml)

install(TARGETS indi_ticfocuser-ng RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_ticfocuser-ng.xml DESTINATION ${INDI_DATA_DIR})
