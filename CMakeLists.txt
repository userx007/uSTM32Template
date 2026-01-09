cmake_minimum_required(VERSION 3.5)
project (stm32app C CXX)

# Putty, Zoc
# add_compile_definitions(SERIAL_TERMINAL)

# RENODE
add_compile_definitions(MY_TERMINAL)

add_subdirectory(sources)

add_executable(${PROJECT_NAME}.elf)

target_link_libraries(${PROJECT_NAME}.elf
    ${LIBOPENCM3_LIB}
    ushell_core
    ushell_core_utils
    ushell_user_root
    uart_access
    user_main
)




