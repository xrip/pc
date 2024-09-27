add_library(psram INTERFACE)

target_sources(psram INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/psram_spi.c
)

target_link_libraries(psram INTERFACE hardware_pio)

target_include_directories(psram INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(psram
  ${CMAKE_CURRENT_LIST_DIR}/psram_spi.pio
)
