cmake_minimum_required(VERSION 4.0)
project(spc VERSION 0.1.0 LANGUAGES C CXX)

############ TOOLS ############
set(RE2C_EXE re2c)

############ PROJECT CONFIG ############
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)


############ BUILD CONFIG ############
set(BUILD_SHARED_LIBS OFF)


set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
#set(LLFIO_DIR ${INCLUDE_DIR}/llfio)
set(MAIN_SRC ${SRC_DIR}/main.cpp)

set(STRIP_RELEASE false)

# Compiler options
set(CONSTEXPR_DEPTH 1000000000)
set(CONSTEXPR_STEPS 1000000000) # Clang only
set(OPTIMIZATION_LEVEL 1)

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
    COMMAND ${RE2C_EXE} ${INPUT} -c -W -i --case-ranges -o ${OUTPUT}
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


# set(PP_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/src)
# 
# set(PP_OUT_FILES)
# 
# file(GLOB_RECURSE PP_INPUT_FILES "${SRC_DIR}/*")
# 
# foreach(INPUT_FILE ${PP_INPUT_FILES})
#   file(RELATIVE_PATH OUTPUT_FILE_RELATIVE ${SRC_DIR} ${INPUT_FILE})
# 
#   set(OUTPUT_FILE ${PP_OUT_DIR}/${OUTPUT_FILE_RELATIVE})
# 
#   cmake_path(GET ${OUTPUT_FILE} PARENT_PATH OUTPUT_FILE_DIR)
# 
#   require_dir(${OUTPUT_FILE_DIR})
# 
#   if (${INPUT_FILE} MATCHES "\.re2c\.(c|h|cpp|hpp)$")
#     re2c_add_command(${INPUT_FILE} ${OUTPUT_FILE})
#   else()
#     non_pp_add_command(${INPUT_FILE} ${OUTPUT_FILE})
#   endif()
# 
#   
#   if (${INPUT_FILE} STREQUAL ${MAIN_FILE})
#     set(MAIN_SRC ${OUTPUT_FILE})
#   else()
#     list(APPEND SRC_MIRROR_FILES ${OUTPUT_FILE})
#   endif()
# endforeach()

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

#add_subdirectory(${LLFIO_DIR} ${CMAKE_BINARY_DIR}/llfio_build)

add_custom_target(pp ALL DEPENDS ${PP_OUT_FILES})


# set(LIBURING_DIR ${CMAKE_SOURCE_DIR}/include/liburing)
# set(LIBURING_SRC_DIR ${LIBURING_DIR}/src)
# set(LIBURING_INCLUDE_DIR ${LIBURING_DIR}/include)
# set(LIBURING_LIB ${LIBURING_SRC_DIR}/liburing.a)


# add_custom_command(
#   OUTPUT ${LIBURING_LIB}
#   COMMAND make library
#   WORKING_DIRECTORY ${LIBURING_DIR}
#   COMMENT "Building liburing static library"
#   VERBATIM
# )
# add_custom_target(liburing_static ALL DEPENDS ${LIBURING_LIB})


############ TARGET ############
message(STATUS "Main source file: ${MAIN_SRC}")
add_executable(spc ${MAIN_SRC})

target_compile_features(spc PRIVATE cxx_std_23)
set_target_properties(spc PROPERTIES LINK_SEARCH_START_STATIC ON)

# target_include_directories(
  # spc
  # BEFORE
  # PRIVATE
  # ${LIBURING_INCLUDE_DIR}
  # ${INCLUDE_DIR}/llfio/include
  # ${INCLUDE_DIR}/outcome/include
  # ${QUICKCPPLIB_DIR}/quickcpplib/include
  # ${RE2C_OUTPUT_DIR}
  # ${SRC_DIR}
# )



# target_link_libraries(spc PRIVATE llfio)
# target_link_libraries(spc PRIVATE ${LIBURING_LIB})



############ LOG INCLUDES ############
get_target_property(LIB_INCLUDES spc INCLUDE_DIRECTORIES)
foreach(DIR ${LIB_INCLUDES})
  message(STATUS "Including ${DIR}")
endforeach()

############ STRIP BINARY ############
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND STRIP_RELEASE)
  add_custom_command(
    TARGET spc
    POST_BUILD
    COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:spc>
    COMMENT "Stripping symbols from $<TARGET_FILE:spc>"
  )
endif()

############ COMPILER COMMON FLAGS ############
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -march=native -O${OPTIMIZATION_LEVEL} -fconstexpr-depth=${CONSTEXPR_DEPTH}") # -fno-pie --static 

############ COMPILER SPECIFIC FLAGS ############
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # using Clang
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-steps=${CONSTEXPR_STEPS}")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using GCC
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # using Intel C++
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using Visual Studio C++
endif()

############ ENABLE COLOR ############
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
