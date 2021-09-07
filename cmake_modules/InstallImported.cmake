
function (install_imported)

    cmake_parse_arguments (ARG "" "DESTINATION" "TARGETS" ${ARGN})

    if (NOT DEFINED ARG_DESTINATION)
        message (FATAL_ERROR "DESTINATION not defined")
    endif ()

    foreach (target ${ARG_TARGETS})

        get_target_property (location ${target} LOCATION)
        get_target_property (version ${target} VERSION)
        get_target_property (soversion ${target} SOVERSION)
        get_target_property (output_name ${target} OUTPUT_NAME)
        get_target_property (suffix ${target} SUFFIX)
        get_target_property (type ${target} TYPE)

        if (NOT ${type} STREQUAL "SHARED_LIBRARY")
            message (FATAL_ERROR "install_imported: ${type} not supported")
        endif ()

        if (${location} STREQUAL "${target}-NOTFOUND")
            return ()
        endif ()

        if (NOT ${version} STREQUAL "version-NOTFOUND")
            set (version ".${version}")
        else ()
            set (version "")
        endif ()

        if (NOT ${soversion} STREQUAL "soversion-NOTFOUND")
            set (soversion ".${soversion}")
        else ()
            set (soversion "")
        endif ()

        if (${output_name} STREQUAL "output_name-NOTFOUND")
            set (output_name ${target})
        endif ()

        set (name_noversion "${CMAKE_SHARED_LIBRARY_PREFIX}${output_name}${CMAKE_SHARED_LIBRARY_SUFFIX}")

        if (APPLE)
            set (name_version "${CMAKE_SHARED_LIBRARY_PREFIX}${output_name}${version}${CMAKE_SHARED_LIBRARY_SUFFIX}")
            set (name_soversion "${CMAKE_SHARED_LIBRARY_PREFIX}${output_name}${soversion}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        else ()
            set (name_version "${CMAKE_SHARED_LIBRARY_PREFIX}${output_name}${CMAKE_SHARED_LIBRARY_SUFFIX}${version}")
            set (name_soversion "${CMAKE_SHARED_LIBRARY_PREFIX}${output_name}${CMAKE_SHARED_LIBRARY_SUFFIX}${soversion}")
        endif ()

        if (NOT IS_ABSOLUTE ${location})
            set (location ${CMAKE_CURRENT_SOURCE_DIR}/${location})
        endif ()

        if (NOT ${name_noversion} STREQUAL ${name_soversion})
            add_custom_command (
                OUTPUT ${name_noversion}
                COMMAND ${CMAKE_COMMAND} -E create_symlink ${name_soversion} ${name_noversion} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                MAIN_DEPENDENCY ${name_soversion}
            )

            install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${name_noversion} DESTINATION ${ARG_DESTINATION})
        endif ()

        if (NOT ${name_soversion} STREQUAL ${name_version})
            add_custom_command (
                OUTPUT ${name_soversion}
                COMMAND ${CMAKE_COMMAND} -E create_symlink ${name_version} ${name_soversion} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                MAIN_DEPENDENCY ${name_version}
            )

            install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${name_soversion} DESTINATION ${ARG_DESTINATION})
        endif ()

        add_custom_command (
            OUTPUT ${name_version}
            COMMAND ${CMAKE_COMMAND} -E copy "${location}" "${CMAKE_CURRENT_BINARY_DIR}/${name_version}"
            MAIN_DEPENDENCY ${location}
        )

        install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${name_version} DESTINATION ${ARG_DESTINATION})

        add_custom_target(
            imported_${output_name} ALL
            DEPENDS ${name_version} ${name_noversion} ${name_soversion}
        )

    endforeach ()

endfunction ()
