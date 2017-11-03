cmake_minimum_required(VERSION 3.8)
project(socket_example)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES main.cpp)
add_executable(socket_example ${SOURCE_FILES})