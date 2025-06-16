cmake_minimum_required(VERSION 3.5)
project (stm32app C CXX)

add_subdirectory(sources)

add_executable(${PROJECT_NAME}.elf)

target_link_libraries(${PROJECT_NAME}.elf
    ${LIBOPENCM3_LIB}
    user_main
    user_lib
)



