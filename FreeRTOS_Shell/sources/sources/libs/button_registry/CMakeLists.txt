cmake_minimum_required(VERSION 3.3)
project(button_registry)


add_library(${PROJECT_NAME}
    OBJECT
        src/ButtonRegistry.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
       inc
)

