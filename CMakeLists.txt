cmake_minimum_required(VERSION 3.10)
project(bloa_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BLOA_USE_CURL "Enable curl support" ON)
option(BLOA_USE_SQLITE "Enable sqlite3 support" ON)

if (BLOA_USE_CURL)
  find_package(CURL REQUIRED)
endif()

if (BLOA_USE_SQLITE)
  find_package(SQLite3 REQUIRED)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(bloa
    src/main.cpp
    src/parser.cpp
    src/interpreter.cpp
    src/stdlib.cpp
)

if (BLOA_USE_CURL)
  target_link_libraries(bloa PRIVATE CURL::libcurl)
endif()

if (BLOA_USE_SQLITE)
  target_include_directories(bloa PRIVATE ${SQLite3_INCLUDE_DIRS})
  target_link_libraries(bloa PRIVATE ${SQLite3_LIBRARIES})
endif()
