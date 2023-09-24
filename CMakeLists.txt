cmake_minimum_required(VERSION 3.15)

project(Perf C CXX)

SET(CMAKE_CXX_COMPILER "g++")
SET(CMAKE_C_COMPILER "gcc")

add_compile_options(-Wall -Wno-sign-compare -Wno-unused-function -Werror -O3 -g -std=c++20)

add_library(Perf SHARED RuntimePerf.cpp)
install(TARGETS Perf)

add_subdirectory(test)