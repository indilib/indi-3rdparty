cmake_minimum_required(VERSION 3.16)

project(gui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(QT_REQUIRED_COMPONENTS
    Core
    Gui
    Widgets
)

set(QT_ADD_RESOURCES_COMMAND "")

find_package(Qt6 COMPONENTS ${QT_REQUIRED_COMPONENTS})

if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS ${QT_REQUIRED_COMPONENTS})

    if(NOT Qt5_FOUND)
        message(FATAL_ERROR "No version of Qt (Qt6 or Qt5 >= 5.15) was found with all required components.")
    else()
        set(QT_MAJOR_VERSION 5)
        set(QT_ADD_RESOURCES_COMMAND qt5_add_resources)
    endif()
else()
    set(QT_MAJOR_VERSION 6)
    set(QT_ADD_RESOURCES_COMMAND qt6_add_resources)
endif()

use_cxx11()

set (CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories (
    "${CMAKE_CURRENT_SOURCE_DIR}/qt"
)

configure_file (gui_info.rc.in gui_info.rc)

if (POLOLU_BUILD)
  if (APPLE)
    # Embed app.icns in the executable.
    ${QT_ADD_RESOURCES_COMMAND}(ICON_QRC qt/app_icns.qrc)
  else ()
    # Embed app.ico in the executable.
    ${QT_ADD_RESOURCES_COMMAND}(ICON_QRC qt/app_ico.qrc)
  endif ()
endif ()

add_executable (gui
  main.cpp
  main_controller.cpp
  qt/bootloader_window.cpp
  qt/main_window.cpp
  qt/BallScrollBar.cpp
  qt/InputWizard.cpp
  qt/current_spin_box.cpp
  qt/elided_label.cpp
  qt/time_spin_box.cpp
  to_string.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/gui_info.rc
  ${ICON_QRC}
  ${ICON_FILE}
)

set_target_properties (gui PROPERTIES
  OUTPUT_NAME ${GUI_NAME}
)

if (WIN32)
  set_target_properties (gui PROPERTIES
    LINK_FLAGS "-mwindows"
  )
endif ()

target_link_libraries(gui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    lib
    bootloader
)

install(TARGETS gui DESTINATION bin)
