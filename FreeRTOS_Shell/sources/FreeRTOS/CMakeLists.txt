cmake_minimum_required(VERSION 3.5)

# FreeRTOS Library
project(freertos C)

# ============== PORT CONFIGURATION ==============
# This should be set by the parent CMakeLists.txt
if(NOT DEFINED FREERTOS_PORT)
    message(FATAL_ERROR "FREERTOS_PORT must be defined by parent CMakeLists.txt")
endif()

if(NOT DEFINED FREERTOS_CONFIG_DIR)
    message(FATAL_ERROR "FREERTOS_CONFIG_DIR must be defined by parent CMakeLists.txt")
endif()

# Validate that the port directory exists
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/portable/${FREERTOS_PORT}")
    message(FATAL_ERROR "FreeRTOS port directory not found: ${CMAKE_CURRENT_SOURCE_DIR}/portable/${FREERTOS_PORT}")
endif()

# Validate that FreeRTOSConfig.h exists
if(NOT EXISTS "${FREERTOS_CONFIG_DIR}/FreeRTOSConfig.h")
    message(WARNING "FreeRTOSConfig.h not found at: ${FREERTOS_CONFIG_DIR}/FreeRTOSConfig.h")
endif()

message(STATUS "FreeRTOS Configuration:")
message(STATUS "  Port: ${FREERTOS_PORT}")
message(STATUS "  Config Dir: ${FREERTOS_CONFIG_DIR}")
message(STATUS "  Heap: ${FREERTOS_HEAP}")

# ============== SOURCE FILES ==============
# Core FreeRTOS source files
set(FREERTOS_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/timers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/event_groups.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_buffer.c
)

# Port-specific source files
set(FREERTOS_PORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/portable/${FREERTOS_PORT}/port.c
)

# Memory management (heap) - default to heap_4 if not specified
if(NOT DEFINED FREERTOS_HEAP)
    set(FREERTOS_HEAP "heap_4")
endif()

set(FREERTOS_HEAP_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/portable/MemMang/${FREERTOS_HEAP}.c
)

# Verify heap file exists
if(NOT EXISTS ${FREERTOS_HEAP_SOURCE})
    message(FATAL_ERROR "FreeRTOS heap implementation not found: ${FREERTOS_HEAP_SOURCE}")
endif()

# ============== CREATE LIBRARY ==============
add_library(freertos STATIC
    ${FREERTOS_CORE_SOURCES}
    ${FREERTOS_PORT_SOURCES}
    ${FREERTOS_HEAP_SOURCE}
)

# ============== INCLUDE DIRECTORIES ==============
# Public include directories (propagated to linking targets)
target_include_directories(freertos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/portable/${FREERTOS_PORT}
    ${FREERTOS_CONFIG_DIR}
)

# ============== COMPILER OPTIONS ==============
target_compile_options(freertos PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    $<$<CONFIG:DEBUG>:-Og -g3>
    $<$<CONFIG:RELEASE>:-O2>
)

# ============== DEFINITIONS ==============
# Add any FreeRTOS-specific definitions here if needed
# target_compile_definitions(freertos PRIVATE ...)

# ============== DISPLAY INFO ==============
message(STATUS "FreeRTOS library created successfully")
message(STATUS "  Core sources: ${FREERTOS_CORE_SOURCES}")
message(STATUS "  Port sources: ${FREERTOS_PORT_SOURCES}")
message(STATUS "  Heap source: ${FREERTOS_HEAP_SOURCE}")