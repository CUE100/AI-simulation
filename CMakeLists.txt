cmake_minimum_required(VERSION 3.16)

project(AI_simulation
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ai_simulation
    engine/main.cpp
)

target_include_directories(ai_simulation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine
)

if(MSVC)
    target_compile_options(ai_simulation PRIVATE /W4)
else()
    target_compile_options(ai_simulation PRIVATE -Wall -Wextra -Wpedantic)
endif()
