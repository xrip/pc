add_library(hdmi INTERFACE)

target_sources(hdmi INTERFACE ${CMAKE_CURRENT_LIST_DIR}/hdmi.c)

target_link_libraries(hdmi INTERFACE hardware_pio hardware_clocks hardware_dma)

target_include_directories(hdmi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

