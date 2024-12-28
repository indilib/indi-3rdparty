include(FindPackageHandleStandardArgs)

# Define GPIOD components
set(GPIOD_VALID_COMPONENTS C CXX)

# Set component dependencies
set(GPIOD_CXX_DEPENDS C)

# Function to handle component search
function(_gpiod_find_component component)
    string(TOUPPER "${component}" upper_comp)
    
    if(component STREQUAL "C")
        # Find C library and headers
        find_path(GPIOD_C_INCLUDE_DIR
            NAMES gpiod.h
            PATH_SUFFIXES gpiod
        )
        find_library(GPIOD_C_LIBRARY
            NAMES gpiod
        )
        
        if(GPIOD_C_LIBRARY AND GPIOD_C_INCLUDE_DIR)
            set(GPIOD_${upper_comp}_FOUND TRUE PARENT_SCOPE)
            set(GPIOD_C_LIBRARIES ${GPIOD_C_LIBRARY} PARENT_SCOPE)
            set(GPIOD_C_INCLUDE_DIRS ${GPIOD_C_INCLUDE_DIR} PARENT_SCOPE)
            
            if(NOT TARGET GPIOD::C)
                add_library(GPIOD::C UNKNOWN IMPORTED)
                set_target_properties(GPIOD::C PROPERTIES
                    IMPORTED_LOCATION "${GPIOD_C_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${GPIOD_C_INCLUDE_DIR}"
                )
            endif()
        endif()
        
    elseif(component STREQUAL "CXX")
        # Find C++ library and headers
        find_path(GPIOD_CXX_INCLUDE_DIR
            NAMES gpiod.hpp
            PATH_SUFFIXES gpiod
        )
        find_library(GPIOD_CXX_LIBRARY
            NAMES gpiodcxx
        )
        
        if(GPIOD_CXX_LIBRARY AND GPIOD_CXX_INCLUDE_DIR)
            set(GPIOD_${upper_comp}_FOUND TRUE PARENT_SCOPE)
            set(GPIOD_CXX_LIBRARIES ${GPIOD_CXX_LIBRARY} PARENT_SCOPE)
            set(GPIOD_CXX_INCLUDE_DIRS ${GPIOD_CXX_INCLUDE_DIR} PARENT_SCOPE)
            
            if(NOT TARGET GPIOD::CXX)
                add_library(GPIOD::CXX UNKNOWN IMPORTED)
                set_target_properties(GPIOD::CXX PROPERTIES
                    IMPORTED_LOCATION "${GPIOD_CXX_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${GPIOD_CXX_INCLUDE_DIR}"
                    INTERFACE_LINK_LIBRARIES "GPIOD::C"
                )
            endif()
        endif()
    endif()
endfunction()

# Handle components
if(NOT GPIOD_FIND_COMPONENTS)
    # If no components specified, find all
    set(GPIOD_FIND_COMPONENTS ${GPIOD_VALID_COMPONENTS})
endif()

# Validate components
foreach(comp IN LISTS GPIOD_FIND_COMPONENTS)
    if(NOT comp IN_LIST GPIOD_VALID_COMPONENTS)
        message(FATAL_ERROR "Invalid GPIOD component: ${comp}")
    endif()
endforeach()

# Find each component and its dependencies
foreach(comp IN LISTS GPIOD_FIND_COMPONENTS)
    if(NOT GPIOD_${comp}_FOUND)
        # Check dependencies
        if(DEFINED GPIOD_${comp}_DEPENDS)
            foreach(dep IN LISTS GPIOD_${comp}_DEPENDS)
                if(NOT dep IN_LIST GPIOD_FIND_COMPONENTS)
                    list(APPEND GPIOD_FIND_COMPONENTS ${dep})
                    _gpiod_find_component(${dep})
                endif()
            endforeach()
        endif()
        
        _gpiod_find_component(${comp})
    endif()
endforeach()

# Set GPIOD_LIBRARIES and GPIOD_INCLUDE_DIRS
set(GPIOD_LIBRARIES)
set(GPIOD_INCLUDE_DIRS)
foreach(comp IN LISTS GPIOD_FIND_COMPONENTS)
    if(GPIOD_${comp}_FOUND)
        if(GPIOD_${comp}_LIBRARIES)
            list(APPEND GPIOD_LIBRARIES ${GPIOD_${comp}_LIBRARIES})
        endif()
        if(GPIOD_${comp}_INCLUDE_DIRS)
            list(APPEND GPIOD_INCLUDE_DIRS ${GPIOD_${comp}_INCLUDE_DIRS})
        endif()
    endif()
endforeach()

# Remove duplicate include directories
list(REMOVE_DUPLICATES GPIOD_INCLUDE_DIRS)

# Handle REQUIRED, QUIET, etc.
find_package_handle_standard_args(GPIOD
    REQUIRED_VARS GPIOD_LIBRARIES GPIOD_INCLUDE_DIRS
    HANDLE_COMPONENTS
)

mark_as_advanced(
    GPIOD_C_INCLUDE_DIR
    GPIOD_C_LIBRARY
    GPIOD_CXX_INCLUDE_DIR
    GPIOD_CXX_LIBRARY
)
