cmake_minimum_required(VERSION 3.3)
project(sys_info)


add_library(${PROJECT_NAME}
    OBJECT
        src/sys_info.c
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        freertos
        ushell_core_config
)
