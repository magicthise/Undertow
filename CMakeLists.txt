cmake_minimum_required(VERSION 3.20)
project(Undertow VERSION 0.1.0 LANGUAGES CXX)

# C++17 so the engine can also build inside VCV Rack's plugin toolchain.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UNDERTOW_BUILD_CLI "Build the undertow-render command-line tool" ON)
option(UNDERTOW_BUILD_TESTS "Build the engine tests" ON)

add_subdirectory(engine)

if(UNDERTOW_BUILD_CLI)
  add_subdirectory(cli)
endif()

if(UNDERTOW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
