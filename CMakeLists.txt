cmake_minimum_required(VERSION 3.5.0)
project(spc VERSION 0.1.0 LANGUAGES C CXX)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(MAIN_RE2C_SRC ${SRC_DIR}/main.re2c.cpp)
set(MAIN_SRC ${SRC_DIR}/main.cpp)

add_custom_command(
  OUTPUT ${SRC_DIR}/main.cpp
  COMMAND re2c ${MAIN_RE2C_SRC} -c -W -i --case-ranges -o ${MAIN_SRC}
  DEPENDS ${MAIN_RE2C_SRC}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(spc ${MAIN_SRC})

target_compile_features(spc PRIVATE cxx_std_20)

set(CMAKE_CXX_FLAGS "-O3")
#set(CMAKE_CXX_FLAGS "-pg -no-pie")