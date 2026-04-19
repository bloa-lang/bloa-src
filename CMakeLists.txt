cmake_minimum_required(VERSION 3.10)
project(bloa_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BLOA_USE_CURL "Enable curl support" ON)
option(BLOA_USE_SQLITE "Enable sqlite3 support" ON)
option(BLOA_USE_MYSQL "Enable MySQL client support" ON)

if (BLOA_USE_CURL)
  find_package(CURL REQUIRED)
endif()

if (BLOA_USE_SQLITE)
  find_package(SQLite3 REQUIRED)
endif()

if (BLOA_USE_MYSQL)
  find_path(MySQL_INCLUDE_DIR mysql/mysql.h)
  find_library(MySQL_LIBRARIES mysqlclient)
  if (NOT MySQL_INCLUDE_DIR OR NOT MySQL_LIBRARIES)
    message(FATAL_ERROR "MySQL support requested, but mysqlclient headers or library not found")
  endif()
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

if (BLOA_USE_MYSQL)
  target_include_directories(bloa PRIVATE ${MySQL_INCLUDE_DIR})
  target_link_libraries(bloa PRIVATE ${MySQL_LIBRARIES})
  target_compile_definitions(bloa PRIVATE BLOA_USE_MYSQL=1)
endif()

add_custom_target(check
  COMMAND bash "${CMAKE_SOURCE_DIR}/tests/run_tests.sh" "$<TARGET_FILE:bloa>"
  DEPENDS bloa
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
