cmake_minimum_required(VERSION 3.3)
project(ao_defs)


add_library(${PROJECT_NAME}
    OBJECT
        src/ao_defs.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
       inc
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBOPENCM3_LIB}
        ao_config
)