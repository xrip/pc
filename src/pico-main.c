#include <pico/time.h>
#include "hardware/clocks.h"
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/stdio.h>
#include <pico/multicore.h>

#include "emulator/emulator.h"

#include "audio.h"
#include "graphics.h"
#include "ps2.h"
#include "ff.h"
#include "psram_spi.h"

FATFS fs;
i2s_config_t i2s_config;

void tandy_write(uint16_t reg, uint8_t value) {
}

extern void adlib_getsample(int16_t *sndptr, intptr_t numsamples);
extern void adlib_init(uint32_t samplerate);
extern void adlib_write(uintptr_t idx, uint8_t val);

void adlib_write_d(uint16_t reg, uint8_t value) {
    //adlib_write(reg, value);
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

#define AUDIO_BUFFER_LENGTH (SOUND_FREQUENCY /60 +1)
static int16_t audio_buffer[AUDIO_BUFFER_LENGTH * 2] = { 0 };
static int sample_index = 0;

/* Renderer loop on Pico's second core */
void __time_critical_func() second_core() {
    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = AUDIO_BUFFER_LENGTH;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
    sleep_ms(100);

    graphics_init();
    graphics_set_buffer(VIDEORAM, 320, 200);
    graphics_set_textbuffer(VIDEORAM + 32768);
    graphics_set_bgcolor(0);
    graphics_set_offset(0, 0);
    graphics_set_flashmode(false, false);

    for (int i = 0; i < 256; ++i) {
        graphics_set_palette(i, vga_palette[i]);
    }

    graphics_set_mode(videomode);

    sem_acquire_blocking(&vga_start_semaphore);

    uint64_t tick = time_us_64();
    uint64_t last_timer_tick = tick, last_cursor_blink = tick, last_sound_tick = tick, last_frame_tick = tick, last_raytrace_tick = tick;
    int old_video_mode = videomode;
    int flip;

    uint64_t last_dss_tick = 0;
    int16_t last_dss_sample = 0;

    while (true) {
        if (tick >= last_timer_tick + (1000000 / timer_period)) {
            doirq(0);
            last_timer_tick = tick;
        }

        if (tick >= last_cursor_blink + 500000) {
            cursor_blink_state ^= 1;
            last_cursor_blink = tick;
        }

        // Dinse Sound Source frequency 7100
        if (tick > last_dss_tick + 140) {
            last_dss_sample = dss_sample() * 32;

            last_dss_tick = tick;
        }

        // Sound frequency 44100
        if (tick > last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            static int sound_counter = 0;
            int16_t samples[2] = { 0, 0 };
            //adlib_getsample((int16_t *) &samples, 1);
            if (last_dss_sample)
                samples[0] += last_dss_sample;
            if (speakerenabled)
                samples[0] += speaker_sample();

            samples[0] += sn76489_sample();



            samples[1] = samples[0];

            cms_samples(samples);


            audio_buffer[sample_index++] = samples[1];
            audio_buffer[sample_index++] = samples[0];


            if (sample_index >= AUDIO_BUFFER_LENGTH * 2) {
                sample_index = 0;
                i2s_dma_write(&i2s_config, audio_buffer);
                //active_buffer ^= 1;
            }

            last_sound_tick = tick;
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
                    default: {
                        for (int i = 0; i < 16; ++i) {
                            graphics_set_palette(i, cga_palette[i]);
                        }
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
extern bool PSRAM_AVAILABLE;
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
    i2s_config = i2s_get_default_config();

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);

    init_psram();

    if (FR_OK != f_mount(&fs, "0", 1)) {
        draw_text("SD Card not inserted or SD Card error!", 0, 0, 12, 0);
        while (1);
    }
   // adlib_init(SOUND_FREQUENCY);
    sn76489_reset();
    reset86();

    while (true) {
        exec86(32768);
        tight_loop_contents();
    }


}