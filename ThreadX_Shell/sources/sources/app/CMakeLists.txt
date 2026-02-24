cmake_minimum_required(VERSION 3.3)
project(user_main)

add_library(${PROJECT_NAME}
    OBJECT
        src/main.c
        src/app_threadx.c

)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        threadx
        uart_access
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