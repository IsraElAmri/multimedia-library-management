cmake_minimum_required(VERSION 3.10)
project(MultimediaLibraryManagement C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(library_system src/main.c)

if(MSVC)
    target_compile_definitions(library_system PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
