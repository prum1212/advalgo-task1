cmake_minimum_required(VERSION 3.16)
project(multigrep LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(multigrep src/main.cpp src/ahocorasick.cpp)
add_executable(multigrep_tests tests/test_main.cpp src/ahocorasick.cpp)

enable_testing()
add_test(NAME CoreAlgorithmTests COMMAND multigrep_tests)