cmake_minimum_required(VERSION 3.22)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(286 C CXX ASM)
enable_language(CXX)
SET(BUILD_NAME "${PROJECT_NAME}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${PICO_PLATFORM}/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC "src/emulator/*.cpp" "src/emulator/*.c" "src/emu8950/*.c" "src/emu8950/*.cpp")

message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_compile_options(-ffunction-sections -fdata-sections -fms-extensions -O2)
#add_compile_options(-fms-extensions -O2)

if (PICO_PLATFORM STREQUAL "host")
    add_executable(${PROJECT_NAME} ${SRC} src/win32-main.cpp src/WinMiniFB.c src/printf/printf.c)

    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
else ()
    pico_sdk_init()

    add_executable(${PROJECT_NAME} ${SRC} src/pico-main.c src/printf/printf.c src/74hc595/74hc595.c)
    target_sources(${PROJECT_NAME} PRIVATE src/emu8950/slot_render_pico.S)

    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
    pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})

    add_subdirectory(drivers/psram)
    target_link_libraries(${PROJECT_NAME} PRIVATE psram)

    if (PICO_PLATFORM STREQUAL "rp2040")
        add_subdirectory(memops_opt)
        target_link_libraries(${PROJECT_NAME} PRIVATE psram memops_opt)
        SET(BUILD_NAME "${BUILD_NAME}-RP2040")
        target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
    else ()
        add_compile_options(-flto -fwhole-program)
        SET(BUILD_NAME "${BUILD_NAME}-RP2350")
        target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=3)
    endif ()
    pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)


    pico_set_program_name(${PROJECT_NAME} "80286 PC emulator by xrip & DnCraptor")
    pico_set_program_version(${PROJECT_NAME} "wip")

    add_subdirectory(drivers/ps2)
    add_subdirectory(drivers/fatfs)
    add_subdirectory(drivers/sdcard)
    add_subdirectory(drivers/vga-nextgen)
    add_subdirectory(drivers/hdmi)
    add_subdirectory(drivers/graphics)
    add_subdirectory(drivers/audio)
    add_subdirectory(drivers/nespad)

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGA)
    target_link_libraries(${PROJECT_NAME} PRIVATE vga-nextgen)
    SET(BUILD_NAME "${BUILD_NAME}-VGA")

#        target_link_libraries(${PROJECT_NAME} PRIVATE hdmi)
#        target_compile_definitions(${PROJECT_NAME} PRIVATE HDMI)
#        SET(BUILD_NAME "${BUILD_NAME}-HDMI")

    target_link_libraries(${PROJECT_NAME} PRIVATE
            ps2
            sdcard
            audio
            graphics
            nespad

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
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
            #            PICO_HEAP_SIZE=0
            #            USE_ZONE_FOR_MALLOC=1
            PICO_FLASH_SIZE_BYTES=16777216

            RP2350_SPEED=444
            RP2040_SPEED=396

            #ONBOARD_PSRAM

            # VGA 8 pins starts from pin:
            VGA_BASE_PIN=6

            # HDMI 8 pins starts from pin:
            HDMI_BASE_PIN=6

            # SDCARD
            SDCARD_PIN_SPI0_CS=5
            SDCARD_PIN_SPI0_SCK=2
            SDCARD_PIN_SPI0_MOSI=3
            SDCARD_PIN_SPI0_MISO=4

            PSRAM_SPINLOCK=1
            PSRAM_ASYNC=1

            PSRAM_PIN_CS=18
            PSRAM_PIN_SCK=19
            PSRAM_PIN_MOSI=20
            PSRAM_PIN_MISO=21

            # NES Gamepad
            USE_NESPAD
            NES_GPIO_CLK=14
            NES_GPIO_DATA=16
            NES_GPIO_LAT=15

            #HARDWARE_SOUND
            CLOCK_PIN=17
            PCM_PIN=22

           I2S_SOUND

            #PWM_SOUND
            PWM_LEFT_CHANNEL=26
            PWM_RIGHT_CHANNEL=27
            PWM_BEEPER=28

            #TOTAL_VIRTUAL_MEMORY_KBS=8192
    )

    pico_add_extra_outputs(${PROJECT_NAME})
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        USE_EMU8950_OPL
        EMU8950_SLOT_RENDER=1
        EMU8950_NO_RATECONV=1
        # slightly slower but only uses 1K of sin tables vs 9K
        EMU8950_NO_WAVE_TABLE_MAP=1

        EMU8950_NO_TLL=1 # don't use lookup table for total level
        EMU8950_NO_FLOAT=1 # double check there is no float
        EMU8950_NO_TIMER=1 # disable timer which isn't used
        EMU8950_NO_TEST_FLAG=1 # disable test flags (which aren't used)
        EMU8950_SIMPLER_NOISE=1 # only generate noise bit when needed
        EMU8950_SHORT_NOISE_UPDATE_CHECK=1 # only update short noise if it is used
        # actually this doesn't make things faster
        #EMU8950_LINEAR_NEG_NOT_NOT=1 # negative values rounded towrds -infinity not 0 without this; does it matter?

        EMU8950_LINEAR_SKIP=1 # skip silent slots
        EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION # early out envelope when DECAY/SUSTAIN/RELEASE envelope reaches mute
        EMU8950_NO_PERCUSSION_MODE=1 # rhythm only mode (doom doesn't use percussion; whhhaaaah!?)
        EMU8950_LINEAR=1 # reorganize to do linear runs of channels

        # things we really don't care about but need for diff-ing with non linear version
        # BEWARE - need to turn this off for comparison with on device version which doesn't do nitpicks (though could i guess)

        EMU8950_ASM=1

)
target_include_directories(${PROJECT_NAME} PRIVATE src src/emu8950)
target_include_directories(${PROJECT_NAME} PRIVATE src/printf)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections)