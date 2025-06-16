
cmake_minimum_required(VERSION 3.10)
project(user_main)


message(STATUS "LIBOPENCM3_INC = ${LIBOPENCM3_INC}")


add_library(${PROJECT_NAME}
    OBJECT
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        user_lib
        ${LIBOPENCM3_LIB}
)

