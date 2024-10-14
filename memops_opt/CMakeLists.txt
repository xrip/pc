add_library(memops_opt INTERFACE)

target_sources(memops_opt INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/memcpy_armv6m.S
    ${CMAKE_CURRENT_LIST_DIR}/memops_opt.c
)

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(memops_opt INTERFACE
    pico_stdlib
)

if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/test")
    # Check if template for tests exists
    add_subdirectory(test)
endif()
