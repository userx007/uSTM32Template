cmake_minimum_required(VERSION 3.5)

project(ushell_core)

add_library(${PROJECT_NAME}
    OBJECT
        src/ushell_core.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc

)

target_link_libraries(${PROJECT_NAME}
    ushell_core_utils
    ushell_core_config
)
