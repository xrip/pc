add_library(ws2812 INTERFACE)

target_sources(ws2812 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/ws2812.c
        ${CMAKE_CURRENT_LIST_DIR}/ws2812.h
)

target_link_libraries(ws2812 INTERFACE hardware_pio hardware_clocks)

target_include_directories(ws2812 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(ws2812
        ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)
