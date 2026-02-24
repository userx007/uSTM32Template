cmake_minimum_required(VERSION 3.3)
project(user_main)

add_library(${PROJECT_NAME}
    OBJECT
        src/main.c
        src/app_threadx.cpp

)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        threadx
        hd44780
        uart_access
        ushell_core
        ushell_core_utils
        ushell_user_root        
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        src 
        threadx/common/inc
)

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE 
        TX_INCLUDE_USER_DEFINE_FILE
)