cmake_minimum_required(VERSION 3.10)

project(PakitReceiver C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

file(GLOB SOURCES "src/*.c")

add_library(packet_receiver ${SOURCES})

add_executable(simple_receiver examples/simple_receiver.c)
target_link_libraries(simple_receiver packet_receiver)