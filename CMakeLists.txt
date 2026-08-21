cmake_minimum_required(VERSION 3.20)
project(maxRTOS C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(maxrtos)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(demo)
endif()