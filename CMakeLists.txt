cmake_minimum_required(VERSION 3.1.0)

project(spli)

set(CMAKE_CXX_COMPILER "c++")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy;") # all checks enabled

if(MSVC)
	add_compile_options(/W4 "$<$<CONFIG:DEBUG>:/Zi;/Od>")
else()
	add_compile_options(-Wall -Wextra -Wpedantic -Wconversion "$<$<CONFIG:DEBUG>:-O0;-ggdb>")
endif()

add_executable(spli src/main.cpp)
set_property(TARGET spli PROPERTY C_STANDARD 11)
set_property(TARGET spli PROPERTY CXX_STANDARD 11)
target_include_directories(spli PRIVATE src/)
