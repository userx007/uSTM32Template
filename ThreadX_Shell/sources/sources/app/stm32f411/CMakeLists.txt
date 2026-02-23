
# --- Application ---
add_executable(${PROJECT_NAME}
    src/main.c
    src/app_threadx.c

)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        threadx
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