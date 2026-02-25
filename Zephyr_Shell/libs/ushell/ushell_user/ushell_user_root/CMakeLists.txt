target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ushell_root_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ushell_root_usercode.cpp
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)
