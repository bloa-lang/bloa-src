cmake_minimum_required(VERSION 3.10)
project(bloa_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(bloa
    src/main.cpp
    src/parser.cpp
    src/interpreter.cpp
)
