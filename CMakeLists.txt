cmake_minimum_required(VERSION 3.22)
include(pico_sdk_import.cmake)

project(pc C CXX ASM)
SET(BUILD_NAME "${PROJECT_NAME}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${PICO_PLATFORM}/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC "src/emulator/*.cpp" "src/emulator/*.c")

message(STATUS "Add source files:")
foreach(SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach()
message(STATUS "")

if (PICO_PLATFORM STREQUAL "host")
    add_executable(${PROJECT_NAME} ${SRC} src/win32-main.cpp src/WinMiniFB.c)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
else ()
    pico_sdk_init()

    add_compile_options(-flto -fwhole-program -ffast-math -ffunction-sections -fdata-sections -O2)
#    add_compile_options(-flto -fwhole-program )
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funroll-loops  -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -Ofast")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops  -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -Ofast")

    add_executable(${PROJECT_NAME} ${SRC} src/pico-main.c)

    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
    pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
    if (PICO_PLATFORM STREQUAL "rp2040")
        SET(BUILD_NAME "${BUILD_NAME}-RP2040")
    target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
    else()
        SET(BUILD_NAME "${BUILD_NAME}-RP2350")
    target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=3)
    endif ()
    pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)

    pico_set_program_name(${PROJECT_NAME} "PC 8086/8088/186/286 emulator")
    pico_set_program_version(${PROJECT_NAME} "wip")

    add_subdirectory(drivers/ps2)
    add_subdirectory(drivers/fatfs)
    add_subdirectory(drivers/sdcard)
    add_subdirectory(drivers/vga-nextgen)
    add_subdirectory(drivers/graphics)
    add_subdirectory(drivers/audio)
    add_subdirectory(drivers/psram)

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGA)

    target_link_libraries(${PROJECT_NAME} PRIVATE
            ps2
            sdcard
            audio
            graphics
            vga-nextgen
            psram

            pico_runtime
            pico_stdlib
            pico_multicore
            hardware_dma
            hardware_pio
            hardware_i2c
            hardware_interp
            hardware_timer
            hardware_clocks
            hardware_pwm
            hardware_flash

            #            tinyusb_host tinyusb_board
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
            PICO_HEAP_SIZE=0
            USE_ZONE_FOR_MALLOC=1
            PICO_FLASH_SIZE_BYTES=16777216

            # VGA 8 pins starts from pin:
            VGA_BASE_PIN=6

            # SDCARD
            SDCARD_PIN_SPI0_CS=5
            SDCARD_PIN_SPI0_SCK=2
            SDCARD_PIN_SPI0_MOSI=3
            SDCARD_PIN_SPI0_MISO=4

            PSRAM
            #PSRAM_MUTEX=1
            PSRAM_SPINLOCK=1
            PSRAM_ASYNC=1

            PSRAM_PIN_CS=18
            PSRAM_PIN_SCK=19
            PSRAM_PIN_MOSI=20
            PSRAM_PIN_MISO=21
    )

    pico_add_extra_outputs(${PROJECT_NAME})
endif ()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections)