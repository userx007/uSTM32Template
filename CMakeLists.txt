cmake_minimum_required(VERSION 3.5)
project(stm32app C CXX)

# Putty, Zoc
# add_compile_definitions(SERIAL_TERMINAL)

# RENODE
add_compile_definitions(MY_TERMINAL)

# Define libopencm3 paths
set(LIBOPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3)
set(LIBOPENCM3_LIB_FILE ${LIBOPENCM3_DIR}/lib/libopencm3_stm32f1.a)

# Create a custom command to build all libopencm3 libraries
add_custom_command(
    OUTPUT ${LIBOPENCM3_LIB_FILE}
    COMMAND make
    WORKING_DIRECTORY ${LIBOPENCM3_DIR}
    COMMENT "Building all libopencm3 libraries..."
)

# Create a custom target that depends on the library file
add_custom_target(libopencm3_build
    DEPENDS ${LIBOPENCM3_LIB_FILE}
)

add_subdirectory(sources)

add_executable(${PROJECT_NAME}.elf)

# Make sure libopencm3 is built before linking
add_dependencies(${PROJECT_NAME}.elf libopencm3_build)

target_link_libraries(${PROJECT_NAME}.elf
    ${LIBOPENCM3_LIB}
    ushell_core
    ushell_core_utils
    ushell_user_root
    uart_access
    user_main
)



