cmake_minimum_required(VERSION 3.28)
project(spc VERSION 0.1.0 LANGUAGES C CXX)

# Project options
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(MAIN_FILE ${SRC_DIR}/main.re2c.cpp)
set(MAIN_SRC ${MAIN_FILE})

set(STRIP_RELEASE true)

# Compiler options
set(CONSTEXPR_DEPTH 1000000000)
set(CONSTEXPR_STEPS 1000000000) # Clang only
set(OPTIMIZATION_LEVEL 3)

function(re2c_compile INPUT OUTPUT)
  message(STATUS "Compiling ${INPUT} to ${OUTPUT}")
  add_custom_command(
    OUTPUT ${OUTPUT}
    COMMAND re2c.exe ${INPUT} -c -W -i --case-ranges -o ${OUTPUT}
    DEPENDS ${INPUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
endfunction()

file(GLOB_RECURSE RE2C_INPUT_FILES "${SRC_DIR}/*re2c.cpp")

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


string(CONCAT RE2C_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR} "/re2c")

foreach(INPUT_FILE ${RE2C_INPUT_FILES})
  file(RELATIVE_PATH OUTPUT_FILE_RELATIVE ${SRC_DIR} ${INPUT_FILE})

  string(CONCAT OUTPUT_FILE ${RE2C_OUTPUT_DIR} "/" ${OUTPUT_FILE_RELATIVE})

  cmake_path(GET OUTPUT_FILE PARENT_PATH OUTPUT_FILE_DIR)

  require_dir(${OUTPUT_FILE_DIR}) 

  re2c_compile(${INPUT_FILE} ${OUTPUT_FILE})

  
  if (${INPUT_FILE} STREQUAL ${MAIN_FILE})
    set(MAIN_SRC ${OUTPUT_FILE})
  else()
    list(APPEND RE2C_OUTPUT_FILES ${OUTPUT_FILE})
  endif()
endforeach()


add_custom_target(re2c ALL DEPENDS ${RE2C_OUTPUT_FILES})

add_executable(spc ${MAIN_SRC})

target_include_directories(
  spc
  BEFORE
  PRIVATE
  ${RE2C_OUTPUT_DIR}
  ${SRC_DIR}
)

target_compile_features(spc PRIVATE cxx_std_23)

get_target_property(LIB_INCLUDES spc INCLUDE_DIRECTORIES)
foreach(DIR ${LIB_INCLUDES})
  message(STATUS "Including ${DIR}")
endforeach()

if(CMAKE_BUILD_TYPE STREQUAL "Release" and STRIP_RELEASE)
  add_custom_command(
    TARGET spc
    POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:spc>
    COMMENT "Stripping symbols from $<TARGET_FILE:spc>"
  )
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-depth=${CONSTEXPR_DEPTH}")

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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O${OPTIMIZATION_LEVEL}")
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)