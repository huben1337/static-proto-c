
function(debug_target _target)
    if(NOT TARGET ${_target})
        message(FATAL_ERROR "debug_target(): '${_target}' is not a valid CMake target!")
    endif()

    message(STATUS "==================================================")
    message(STATUS " DEBUG INFO FOR TARGET: ${_target}")
    message(STATUS "==================================================")

    # 1. Target Type & Basic Info
    get_target_property(_type ${_target} TYPE)
    get_target_property(_source_dir ${_target} SOURCE_DIR)
    get_target_property(_std ${_target} CXX_STANDARD)
    
    message(STATUS "  [Type]              : ${_type}")
    message(STATUS "  [Source Dir]        : ${_source_dir}")
    if(_std)
        message(STATUS "  [C++ Standard]      : C++${_std}")
    endif()

    # 2. Inspect Essential Target Properties
    set(_props_to_check
        INTERFACE_INCLUDE_DIRECTORIES
        INCLUDE_DIRECTORIES
        INTERFACE_COMPILE_OPTIONS
        COMPILE_OPTIONS
        INTERFACE_COMPILE_DEFINITIONS
        COMPILE_DEFINITIONS
        INTERFACE_LINK_LIBRARIES
        LINK_LIBRARIES
        INTERFACE_LINK_OPTIONS
        LINK_OPTIONS
        INTERFACE_COMPILE_FEATURES
        COMPILE_FEATURES
        CXX_VISIBILITY_PRESET
        VISIBILITY_INLINES_HIDDEN
    )

    foreach(_prop IN LISTS _props_to_check)
        get_target_property(_val ${_target} ${_prop})
        if(_val)
            message(STATUS "  [${_prop}]:")
            foreach(_item IN LISTS _val)
                message(STATUS "    - ${_item}")
            endforeach()
        else()
            message(STATUS "  [${_prop}]: <NOT SET>")
        endif()
    endforeach()

    # 3. Print Target Source Files
    get_target_property(_sources ${_target} SOURCES)
    if(_sources)
        message(STATUS "  [SOURCES]:")
        foreach(_src IN LISTS _sources)
            message(STATUS "    - ${_src}")
        endforeach()
    endif()

    message(STATUS "==================================================\n")
endfunction()

# Optional convenience macro to print generator-evaluated compile commands
macro(enable_verbose_target_build _target)
    set_target_properties(${_target} PROPERTIES VERBOSE ON)
endmacro()