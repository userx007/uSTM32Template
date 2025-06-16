
cmake_minimum_required(VERSION 3.10)
project(user_main)

add_library(${PROJECT_NAME}
    OBJECT
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    user_lib
)

