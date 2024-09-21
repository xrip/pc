add_library(vga-nextgen INTERFACE)

target_sources(vga-nextgen INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/vga.c
)

target_link_libraries(vga-nextgen INTERFACE hardware_pio hardware_dma)

target_include_directories(vga-nextgen INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)
