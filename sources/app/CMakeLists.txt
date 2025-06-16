
cmake_minimum_required(VERSION 3.10)
project(user_main)


message(STATUS "LIBOPENCM3_INC = ${LIBOPENCM3_INC}")


add_library(${PROJECT_NAME}
    OBJECT
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBOPENCM3_LIB}
        ushell_core
        ushell_core_utils
        ushell_user_root
)

