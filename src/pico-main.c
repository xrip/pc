#pragma GCC optimize("Ofast")
#include <pico/time.h>
#include "hardware/clocks.h"
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/stdio.h>
#include <pico/multicore.h>
#include "../../memops_opt/memops_opt.h"
#include "emulator/emulator.h"

#include "audio.h"
#include "graphics.h"
#include "ps2.h"
#include "ff.h"
#include "psram_spi.h"
#include "nespad.h"
#include "emu8950.h"
#include "ps2_mouse.h"

FATFS fs;
i2s_config_t i2s_config;
OPL *emu8950_opl;
void tandy_write(uint16_t reg, uint8_t value) {
}

extern void adlib_getsample(int16_t *sndptr, intptr_t numsamples);
extern void adlib_init(uint32_t samplerate);
extern void adlib_write(uintptr_t idx, uint8_t val);

void adlib_write_d(uint16_t reg, uint8_t value) {
    OPL_writeReg(emu8950_opl, reg, value);
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
static int16_t __aligned(4) audio_buffer[AUDIO_BUFFER_LENGTH * 2] = { 0 };
static int sample_index = 0;
extern uint64_t sb_samplerate;
extern uint8_t timeconst;
/* Renderer loop on Pico's second core */
void __time_critical_func() second_core() {
    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = AUDIO_BUFFER_LENGTH;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
    sleep_ms(100);


    emu8950_opl = OPL_new(3579552, SOUND_FREQUENCY);
    graphics_init();
    graphics_set_buffer(VIDEORAM, 320, 200);
    graphics_set_textbuffer(VIDEORAM + 32768);
    graphics_set_bgcolor(0);
    graphics_set_offset(0, 0);
    graphics_set_flashmode(true, true);

    for (uint8_t i = 0; i < 255; i++) {
        graphics_set_palette(i, vga_palette[i]);
    }

    sem_acquire_blocking(&vga_start_semaphore);

    uint64_t tick = time_us_64();
    uint64_t last_timer_tick = tick, last_cursor_blink = tick, last_sound_tick = tick, last_frame_tick = tick;

    uint64_t last_dss_tick = 0;
    uint64_t last_sb_tick = 0;
    int16_t last_dss_sample = 0;
    int16_t last_sb_sample = 0;


    while (true) {
        if (tick >= last_timer_tick + (1000000 / timer_period)) {
            doirq(0);
            last_timer_tick = tick;
        }

        if (tick >= last_cursor_blink + 333333) {
            cursor_blink_state ^= 1;
            last_cursor_blink = tick;
        }

        // Dinse Sound Source frequency 7100
        if (tick > last_dss_tick + (1000000 / 7000)) {
            last_dss_sample = dss_sample();

            last_dss_tick = tick;
        }

#if !PICO_ON_DEVICE
        // Sound Blaster
        if (tick > last_sb_tick + timeconst) {
            last_sb_sample = blaster_sample();

            last_sb_tick = tick;
        }
#endif
        // Sound frequency 44100
        if (tick > last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            int16_t samples[2] = { 0, 0 };
            OPL_calc_buffer_linear(emu8950_opl, (int32_t *)(samples), 1);

            if (last_dss_sample)
                samples[0] += last_dss_sample;
            if (speakerenabled)
                samples[0] += speaker_sample();

            samples[0] += sn76489_sample();

#if !PICO_ON_DEVICE
            if (last_sb_sample)
                samples[0] += last_sb_sample;
#endif
//            samples[0] += adlibgensample() * 32;
            samples[0] += covox_sample;
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
            static uint8_t old_video_mode;
            if (old_video_mode != videomode) {

                if (1) {
                    switch (videomode) {
                        case TGA_160x200x16:
                        case TGA_320x200x16:
                        case TGA_640x200x16: {
                            for (uint8_t i = 0; i < 15; i++) {
                                graphics_set_palette(i, tga_palette[i]);
                            }
                            break;
                        }
                        case COMPOSITE_160x200x16:
                            for (uint8_t i = 0; i < 15; i++) {
                                graphics_set_palette(i, cga_composite_palette[0][i]);
                            }
                            break;

                        case COMPOSITE_160x200x16_force:
                            for (uint8_t i = 0; i < 15; i++) {
                                graphics_set_palette(i, cga_composite_palette[cga_intensity << 1][i]);
                            }
                            break;

                        case CGA_320x200x4_BW:
                        case CGA_320x200x4: {
                            for (uint8_t i = 0; i < 4; i++) {
                                graphics_set_palette(i, cga_palette[cga_gfxpal[cga_colorset][cga_intensity][i]]);
                            }
                            break;
                        }
                        case VGA_320x200x256:
                        case VGA_320x200x256x4:
                        default: {
                            for (uint8_t i = 0; i < 255; i++) {
                                graphics_set_palette(i, vga_palette[i]);
                            }
                        }
                    }
                    graphics_set_mode(videomode);
                    old_video_mode = videomode;
                }


            }
            last_frame_tick = tick;
        }
        tick = time_us_64();
        tight_loop_contents();

    }
    __unreachable();
}
extern bool PSRAM_AVAILABLE;

#if 1
uint8_t __aligned(4) DEBUG_VRAM[80*10] = { 0 };
INLINE void _putchar(char character)
{
    static uint8_t color = 0xf;
    static int x = 0, y = 0;

    if (y == 10) {
        y = 9;
        memmove(DEBUG_VRAM, DEBUG_VRAM + 80, 80 * 9);
        memset(DEBUG_VRAM + 80 * 9, 0, 80);
    }
    uint8_t * vidramptr = DEBUG_VRAM + __fast_mul(y, 80) + x;

    if ((unsigned)character >= 32) {
        if (character >= 96) character -= 32; // uppercase
        *vidramptr = ((character - 32) & 63) | 0 << 6;
        if (x == 80) {
            x = 0;
            y++;
        } else
            x++;
    } else if (character == '\n') {
        x = 0;
        y++;
    } else if (character == '\r') {
        x = 0;
    } else if (character == 8 && x > 0) {
        x--;
        *vidramptr = 0;
    }
}
#endif
int main() {

#if PICO_RP2350
    volatile uint32_t *qmi_m0_timing=(uint32_t *)0x400d000c;
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_50);
    sleep_ms(10);
    *qmi_m0_timing = 0x60007204;
    set_sys_clock_hz(460000000, 0);
    *qmi_m0_timing = 0x60007303;
#else
    memcpy_wrapper_replace(NULL);
    //vreg_set_voltage(VREG_VOLTAGE_1_30);
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    vreg_disable_voltage_limit();
    sleep_ms(33);
    set_sys_clock_khz(378 * 1000, true);
#endif


    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (int i = 0; i < 6; i++) {
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    keyboard_init();
    mouse_init();

    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);

    i2s_config = i2s_get_default_config();

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);

    sleep_ms(50);

    graphics_set_mode(TEXTMODE_80x25_COLOR);

    init_psram();
        if (!PSRAM_AVAILABLE) {
        draw_text("No PSRAM detected.", 0, 0, 12, 0);
//        while (1);
    }

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
    __unreachable();

}