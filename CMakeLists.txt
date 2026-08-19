cmake_minimum_required(VERSION 4.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DebugTarget.cmake)

project(
  spc
  VERSION 0.1.0
  LANGUAGES CXX)

############ PROJECT CONFIG ############
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
#set(CMAKE_BUILD_TYPE "Release")

############ BUILD CONFIG ############
set(BUILD_SHARED_LIBS OFF)

############ TOOLS ############
set(RE2C_EXE re2c)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MAIN_SRC ${SRC_DIR}/main.cpp)

set(STRIP_RELEASE false)

# Compiler options
set(CONSTEXPR_DEPTH 1000000000)
set(CONSTEXPR_STEPS 1000000000) # Clang only
set(OPTIMIZATION_LEVEL 3)

############ EXTERNAL PRE PROCESSING ############
function(non_pp_add_command INPUT OUTPUT)
  message(STATUS "Registered: symlinking ${INPUT} to ${OUTPUT}")
  add_custom_command(
    OUTPUT ${OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${INPUT} ${OUTPUT}
    DEPENDS ${INPUT}
    COMMENT "Symlinking ${INPUT} to ${OUTPUT}"
  )
endfunction()

function(re2c_add_command INPUT OUTPUT)
  message(STATUS "Registered: re2c compiling ${INPUT} to ${OUTPUT}")
  add_custom_command(
    OUTPUT ${OUTPUT}
    COMMAND ${RE2C_EXE} ${INPUT} -c -W -i -o ${OUTPUT}
    DEPENDS ${INPUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "re2c compiling ${INPUT} to ${OUTPUT}"
  )
endfunction()

function(require_dir DIR)
  file(MAKE_DIRECTORY ${DIR})
  # # This is probably slower than letting the os do it
  # if (EXISTS ${DIR})
  #   if (NOT IS_DIRECTORY ${DIR})
  #     message(FATAL_ERROR "${DIR} exists but is not a directory")
  #   endif()
  # else()
  #   file(MAKE_DIRECTORY ${DIR})
  # endif()
endfunction()

set(PP_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/src)

set(PP_OUT_FILES "")

# Recursive function
function(process_directory DIR HAS_PP_OUT)
    # List all entries
    file(GLOB ENTRIES LIST_DIRECTORIES true "${DIR}/*")

    set(NON_PP_DIRS "")
    set(NON_PP_FILES "")

    set(RE2C_FILES "")
    
    set(DIR_SUBDIRS "")

    set(HAS_PP OFF)

    # Separate files and subdirectories
    foreach(DIR_ENTRY ${ENTRIES})
        if(IS_DIRECTORY ${DIR_ENTRY})
            set(HAS_PP_OUT OFF)
            process_directory(${DIR_ENTRY} HAS_PP_OUT)
            if(HAS_PP_OUT)
                set(HAS_PP ON)
            else()
                list(APPEND NON_PP_DIRS ${DIR_ENTRY})
            endif()
        else()
            if(DIR_ENTRY MATCHES "\.re2c\.(c|h|cc|hh|cxx|hxx|cpp|hpp)$")
                list(APPEND RE2C_FILES ${DIR_ENTRY})
                set(HAS_PP ON)
            else()
                list(APPEND NON_PP_FILES ${DIR_ENTRY})
            endif()
        endif()
    endforeach()

    if(HAS_PP)
        file(RELATIVE_PATH DIR_REL ${SRC_DIR} ${DIR})
        file(MAKE_DIRECTORY "${PP_OUT_DIR}/${DIR_REL}")

        foreach(IN_DIR ${NON_PP_DIRS})
            file(RELATIVE_PATH DIR_REL ${SRC_DIR} ${IN_DIR})
            set(OUT_DIR "${PP_OUT_DIR}/${DIR_REL}")
            
            message(STATUS "Registered: dir ${IN_DIR} to ${OUT_DIR}")
            add_custom_command(
                OUTPUT ${OUT_DIR}
                COMMAND ${CMAKE_COMMAND} -E create_symlink ${IN_DIR} ${OUT_DIR}
                COMMENT "Symlinking directory ${RELATIVE_PATH}"
                DEPENDS "${IN_DIR}"
                VERBATIM
            )
            list(APPEND PP_OUT_FILES "${OUT_DIR}")
        endforeach()

        foreach(IN_FILE ${NON_PP_FILES})
            file(RELATIVE_PATH REL_FILE "${SRC_DIR}" "${IN_FILE}")
            set(OUT_FILE "${PP_OUT_DIR}/${REL_FILE}")

            # get_filename_component(OUT_FILE_DIR "${OUT_FILE}" DIRECTORY)
            # file(MAKE_DIRECTORY "${OUT_FILE_DIR}")

            message(STATUS "Registered: symlinking ${IN_FILE} to ${OUT_FILE}")
            add_custom_command(
                OUTPUT "${OUT_FILE}"
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${IN_FILE}" "${OUT_FILE}"
                DEPENDS "${IN_FILE}"
                COMMENT "Symlinking ${REL_FILE}"
            )
            list(APPEND PP_OUT_FILES "${OUT_FILE}")
        endforeach()

        foreach(IN_FILE ${RE2C_FILES})
            file(RELATIVE_PATH REL_FILE "${SRC_DIR}" "${IN_FILE}")
            set(OUT_FILE "${PP_OUT_DIR}/${REL_FILE}")

            # get_filename_component(OUT_FILE_DIR "${OUT_FILE}" DIRECTORY)
            # file(MAKE_DIRECTORY "${OUT_FILE_DIR}")

            re2c_add_command("${IN_FILE}" "${OUT_FILE}")
            list(APPEND PP_OUT_FILES "${OUT_FILE}")
        endforeach()
    else()
    endif()
    set(HAS_PP_OUT ${HAS_PP} PARENT_SCOPE)
    set(PP_OUT_FILES ${PP_OUT_FILES} PARENT_SCOPE)
endfunction()

set(HAS_PP_OUT OFF)
process_directory("${SRC_DIR}" "" HAS_PP_OUT)
if(HAS_PP_OUT)
    file(RELATIVE_PATH MAIN_REL_APTH ${SRC_DIR} ${MAIN_SRC})
    set(MAIN_SRC ${PP_OUT_DIR}/${MAIN_REL_APTH})
endif()

############ SHARED CONFIG TARGET ############
# Define the INTERFACE library that holds all shared build flags, features, and includes
add_library(spc_options INTERFACE)

# C++ Standard Requirements
target_compile_features(spc_options INTERFACE cxx_std_23)

# SANITIZER OPTIONS
set(SANITIZER_FLAG -fsanitize=address,undefined,leak,alignment)
# set(SANITIZER_FLAG "")

set(STL_FLAG -stdlib=libstdc++)

############ COMMON COMPILER FLAGS ############
target_compile_options(
  spc_options
  INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:${STL_FLAG}>
  $<$<COMPILE_LANGUAGE:CXX>:${SANITIZER_FLAG}>
  $<$<COMPILE_LANGUAGE:CXX>:-march=native>
  $<$<COMPILE_LANGUAGE:CXX>:-O${OPTIMIZATION_LEVEL}>
  $<$<COMPILE_LANGUAGE:CXX>:-Werror>
  $<$<COMPILE_LANGUAGE:CXX>:-Wall>
  $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
  $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
  $<$<COMPILE_LANGUAGE:CXX>:-Weverything>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++-compat>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++98-compat>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++98-compat-pedantic>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-pre-c++14-compat>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-pre-c++17-compat>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-pre-c++20-compat>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-newline-eof>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-extra-semi-stmt>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-comma>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-shadow>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-shadow-field>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-shadow-field-in-constructor>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-covered-switch-default>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-padded>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-global-constructors>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-exit-time-destructors>
  # $<$<COMPILE_LANGUAGE:CXX>:-Wno-ctad-maybe-unsupported>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-variable-declarations>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-unsafe-buffer-usage>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-vla-cxx-extension>
  $<$<COMPILE_LANGUAGE:CXX>:-D_GLIBCXX_DEBUG>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fconstexpr-depth=${CONSTEXPR_DEPTH}>
  $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-fconstexpr-steps=${CONSTEXPR_STEPS}>
)

############ COMMON LINKER FLAGS ############
target_link_options(
  spc_options
  INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:${SANITIZER_FLAG}>
  $<$<COMPILE_LANGUAGE:CXX>:${STL_FLAG}>
)

############ EXTERNAL ############
add_subdirectory(
  extern/nameof
  EXCLUDE_FROM_ALL
)


############ INCLUDE ############
# Shared Include Directories
target_include_directories(
  spc_options
  BEFORE
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/nameof/include
)


############ MAIN APPLICATION TARGET ############
message(STATUS "Main source file: ${MAIN_SRC}")
add_executable(spc ${MAIN_SRC})

# Inherit all shared options, includes, std feature, and flags
target_link_libraries(spc PRIVATE spc_options)

set_target_properties(
  spc 
  PROPERTIES
    LINK_SEARCH_START_STATIC ON
)

############ DEBUG ############
debug_target(spc_options)
debug_target(spc)

############ TESTS SUBDIRECTORY ############
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

############ STRIP BINARY ############
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND STRIP_RELEASE)
  add_custom_command(
    TARGET spc
    POST_BUILD
    COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:spc>
    COMMENT "Stripping symbols from $<TARGET_FILE:spc>"
  )
endif()

### CLANG ANALYZER ###
add_custom_target(clang-analyze
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-clang-analyzer.nu
    ${CMAKE_BINARY_DIR}
  WORKING_DIRECTORY
    ${CMAKE_BINARY_DIR}
  COMMENT
    "Running Clang Static Analyzer"
)

### CPPCHECK ###
add_custom_target(cppcheck
  COMMAND
    cppcheck
    -v
    --enable=all
    --inconclusive
    --std=c++23
    -D__AVX2__
    -D__AVX__
    --project=${CMAKE_BINARY_DIR}/compile_commands.json
  WORKING_DIRECTORY
    ${CMAKE_BINARY_DIR}
)