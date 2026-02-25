target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ushell_core.cpp
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)
