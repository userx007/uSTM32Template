
# --- Application ---
add_executable(${PROJECT_NAME}
    src/startup_stm32f4xx.s
    src/tx_initialize_low_level.s
    src/main.c
    src/app_threadx.c
    src/syscalls.c
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        threadx
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        src 
        threadx/common/inc
        threadx_user_cfg
)

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE 
        TX_INCLUDE_USER_DEFINE_FILE
)