cmake_minimum_required(VERSION 3.10)

project(PakitReceiver C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

file(GLOB SOURCES "src/*.c")

add_library(pakit_lib "src/pakit.c")

add_executable(pakit_sample sample/pakit_sample.c)
target_link_libraries(pakit_sample pakit_lib)

add_executable(pakit_test test/pakit_test.c)
target_link_libraries(pakit_test pakit_lib)