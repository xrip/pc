add_library(psram2 INTERFACE)

#target_sources(psram2 INTERFACE
#	${CMAKE_CURRENT_LIST_DIR}/psram_spi2.�
#)

target_link_libraries(psram2 INTERFACE hardware_pio)

target_include_directories(psram2 INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(psram2
  ${CMAKE_CURRENT_LIST_DIR}/psram_spi2.pio
)
