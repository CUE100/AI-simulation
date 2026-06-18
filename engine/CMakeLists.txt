cmake_minimum_required(VERSION 3.16)

project(AI_simulation
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_policy(SET CMP0057 NEW)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sfml")

find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
)

set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assests")

if(EXISTS "${ASSETS_DIR}")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        VERBATIM
    )
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        SFML::System
        SFML::Window
        SFML::Graphics
        SFML::Audio
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
