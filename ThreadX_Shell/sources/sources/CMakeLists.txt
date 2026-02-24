add_subdirectory(libs)
add_subdirectory(ushell)
add_subdirectory(app)

if(STM32_FAMILY STREQUAL "F1")
    add_subdirectory(threadx_port/stm32f103)
else()
    add_subdirectory(threadx_port/stm32f411)
endif()