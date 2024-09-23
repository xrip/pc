#include <pico/time.h>
#include "hardware/clocks.h"
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/stdio.h>
#include <pico/multicore.h>

#include "emulator/emulator.h"

#include "graphics.h"
#include "ps2.h"
#include "ff.h"

FATFS fs;

void tandy_write(uint16_t reg, uint8_t value) {
}

void adlib_write(uint16_t reg, uint8_t value) {

}

void cms_write(uint16_t reg, uint8_t val) {

}

bool handleScancode(uint32_t ps2scancode) {
    port60 = ps2scancode;
    port64 |= 2;
    doirq(1);
    return true;
}
int cursor_blink_state = 0;
struct semaphore vga_start_semaphore;
/* Renderer loop on Pico's second core */
void second_core() {
    graphics_init();
    graphics_set_buffer(VIDEORAM, 320, 200);
    graphics_set_textbuffer(VIDEORAM + 32768);
    graphics_set_bgcolor(0);
    graphics_set_offset(0, 0);
    graphics_set_flashmode(true, true);

    for (int i = 0; i < 256; ++i) {
        graphics_set_palette(i, vga_palette[i]);
    }

    graphics_set_mode(videomode);

    sem_acquire_blocking(&vga_start_semaphore);

    uint64_t tick = time_us_64();
    uint64_t last_timer_tick = tick, last_cursor_blink = tick, last_sound_tick = tick, last_frame_tick = tick, last_raytrace_tick = tick;
    int old_video_mode = videomode;
    int flip;

    while (true) {
        if (tick >= last_timer_tick + timer_period) {
            doirq(0);
            last_timer_tick = tick;
        }

        if (tick >= last_cursor_blink + 500000) {
            cursor_blink_state ^= 1;
            last_cursor_blink = tick;
        }
        if (tick >= last_frame_tick + 16667) {
            if (old_video_mode != videomode) {
                switch (videomode) {
                    case TGA_160x200x16:
                    case TGA_320x200x16:
                    case TGA_640x200x16: {
                        for (int i = 0; i < 16; ++i) {
                            graphics_set_palette(i, tga_palette[i]);
                        }
                        break;
                    }

                    case CGA_320x200x4: {
                        for (int i = 0; i < 4; ++i) {
                            graphics_set_palette(i, cga_palette[cga_gfxpal[cga_colorset][cga_intensity][i]]);
                        }
                        break;
                    }
                    case VGA_320x200x256:
                    case VGA_320x200x256x4: {
                        for (int i = 0; i < 256; ++i) {
                            graphics_set_palette(i, vga_palette[i]);
                        }
                        break;
                    }

                }

                graphics_set_mode(videomode);
                old_video_mode = videomode;
            }
            last_frame_tick = tick;
        }
        tick = time_us_64();
        tight_loop_contents();

    }
}

int main() {
#if PICO_RP2350
    vreg_set_voltage(VREG_VOLTAGE_1_40);
#else
    vreg_set_voltage(VREG_VOLTAGE_1_30);
#endif
    sleep_ms(33);
    set_sys_clock_khz(376 * 1000, true);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (int i = 0; i < 6; i++) {
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    keyboard_init();

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);

    if (FR_OK != f_mount(&fs, "0", 1)) {
        draw_text("SD Card not inserted or SD Card error!", 0, 0, 12, 0);
        while (1);
    }


    reset86();

    while (true) {
        exec86(32768);
    }

    return 0;
}