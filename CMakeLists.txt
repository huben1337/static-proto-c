cmake_minimum_required(VERSION 3.5.0)
project(spc VERSION 0.1.0 LANGUAGES C CXX)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(MAIN_FILE ${SRC_DIR}/main.re2c.cpp)
set(MAIN_SRC ${MAIN_FILE})

function(re2c_compile INPUT OUTPUT)
  message(STATUS "Compiling ${INPUT} to ${OUTPUT}")
  add_custom_command(
    OUTPUT ${OUTPUT}
    COMMAND re2c ${INPUT} -c -W -i --case-ranges -o ${OUTPUT}
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
  PRIVATE
  BEFORE
  ${RE2C_OUTPUT_DIR}
  ${SRC_DIR}
)

target_compile_features(spc PRIVATE cxx_std_20)

set(CMAKE_CXX_FLAGS "-O3")
#set(CMAKE_CXX_FLAGS "-pg -no-pie")