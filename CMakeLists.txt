cmake_minimum_required(VERSION 3.5.0)
project(spc VERSION 0.1.0 LANGUAGES C CXX)

add_executable(spc main.cpp)

target_compile_features(spc PRIVATE cxx_std_20)

set(CMAKE_CXX_FLAGS "-O3")
#set(CMAKE_CXX_FLAGS "-pg -no-pie")