cmake_minimum_required(VERSION 3.5)
project (stm32app)

set(SOURCES sources)

add_executable(${PROJECT_NAME}.elf
    ${SOURCES}
)

target_link_libraries(${PROJECT_NAME}.elf
    ${LIBOPENCM3_LIB}
)



