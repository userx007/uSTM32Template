
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hd44780_pcf8574.cpp
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)