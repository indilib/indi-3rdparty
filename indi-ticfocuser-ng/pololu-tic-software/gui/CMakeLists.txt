cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(gui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(QT_REQUIRED_COMPONENTS
    Core
    Gui
    Widgets
)

find_package(Qt6 COMPONENTS ${QT_REQUIRED_COMPONENTS})

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, looking for Qt5...")
    find_package(Qt5 5.15 COMPONENTS ${QT_REQUIRED_COMPONENTS})

    if(NOT Qt5_FOUND)
        message(STATUS "No version of Qt (Qt6 or Qt5 >= 5.15) was found. The GUI will not be compiled.")
    else()
        message(STATUS "Qt5 (version ${Qt5_VERSION}) found.")
        set(QT_MAJOR_VERSION 5)
        set(QT_FOUND TRUE)
    endif()
else()
    message(STATUS "Qt6 (version ${Qt6_VERSION}) found.")
    set(QT_MAJOR_VERSION 6)
    set(QT_FOUND TRUE)
endif()

if (NOT DEFINED QT_FOUND)
    set(QT_FOUND FALSE)
endif()

use_cxx11()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/qt"
)

if(QT_FOUND)

    configure_file (gui_info.rc.in gui_info.rc)

    if (POLOLU_BUILD)
        if (APPLE)
            if (QT_MAJOR_VERSION EQUAL 6)
                qt6_add_resources(ICON_QRC qt/app_icns.qrc)
            elseif (QT_MAJOR_VERSION EQUAL 5)
                qt5_add_resources(ICON_QRC qt/app_icns.qrc)
            endif ()
        else ()
            if (QT_MAJOR_VERSION EQUAL 6)
                qt6_add_resources(ICON_QRC qt/app_ico.qrc)
            elseif (QT_MAJOR_VERSION EQUAL 5)
                qt5_add_resources(ICON_QRC qt/app_ico.qrc)
            endif ()
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
        Qt::Core
        Qt::Gui
        Qt::Widgets
        lib
        bootloader
    )

    install(TARGETS gui DESTINATION bin)

else()
    message(STATUS "GUI compilation is disabled because no version of Qt was found.")
endif()
