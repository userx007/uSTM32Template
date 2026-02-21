add_subdirectory(libs)
add_subdirectory(ushell)

if(STM32_FAMILY STREQUAL "F1")
    add_subdirectory(app_stm32f103)
else()
    add_subdirectory(app_stm32f411)
endif()