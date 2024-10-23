#pragma GCC optimize("Ofast")
#include <pico/time.h>
#include <hardware/clocks.h>
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/stdio.h>
#include <pico/multicore.h>
#if !PICO_RP2350
#include "psram_spi.h"
#include "../../memops_opt/memops_opt.h"
#else
#include <hardware/structs/qmi.h>
#include <hardware/structs/xip.h>
#endif
#include "emulator/emulator.h"

#include "audio.h"
#include "graphics.h"
#include "ps2.h"
#include "ff.h"
#include "nespad.h"
#include "emu8950.h"
#include "ps2_mouse.h"
#include "74hc595/74hc595.h"


FATFS fs;
i2s_config_t i2s_config;
OPL *emu8950_opl;
void tandy_write(uint16_t reg, uint8_t value) {
#if I2S_SOUND
    sn76489_out(value);
#else
    SN76489_write(value & 0xff);
#endif
}

extern void adlib_getsample(int16_t *sndptr, intptr_t numsamples);
extern void adlib_init(uint32_t samplerate);
extern void adlib_write(uintptr_t idx, uint8_t val);

void adlib_write_d(uint16_t reg, uint8_t value) {
#if I2S_SOUND
    OPL_writeReg(emu8950_opl, reg, value);
#else
    if (reg &1) {
        OPL2_write_byte(0,0, value & 0xff);
    } else {
        OPL2_write_byte(1,0, value & 0xff);
    }
#endif
}

void cms_write(uint16_t reg, uint8_t val) {
#if I2S_SOUND
    cms_out(reg, val);
#else
    switch (reg - 0x220) {
        case 0: SAA1099_write(0, 0, val & 0xf); break;
        case 1: SAA1099_write(0, 1, val & 0xff); break;
        case 3: SAA1099_write(1, 0, val & 0xf); break;
        case 4: SAA1099_write(1, 1, val & 0xff); break;

    }
#endif
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
static int16_t __aligned(4) audio_buffer[2][AUDIO_BUFFER_LENGTH * 2] = { 0 };
int active_buffer = 0;
static int sample_index = 0;
extern uint64_t sb_samplerate;
extern uint16_t timeconst;
/* Renderer loop on Pico's second core */
void __time_critical_func() second_core() {
#if I2S_SOUND
    i2s_config = i2s_get_default_config();
    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = AUDIO_BUFFER_LENGTH;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
#else

#endif
    sleep_ms(100);

    emu8950_opl = OPL_new(3579552, SOUND_FREQUENCY);

    blaster_reset();

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
        if (tick >= last_timer_tick + timer_period) {
            doirq(0);
            last_timer_tick = tick;
        }

        if (tick >= last_cursor_blink + 333333) {
            cursor_blink_state ^= 1;
            last_cursor_blink = tick;
        }
#if I2S_SOUND
#if 1 || !PICO_ON_DEVICE
        // Sound Blaster
        if (tick >= last_sb_tick + timeconst) {
            last_sb_sample = blaster_sample();

            last_sb_tick = tick;
        }
#endif


        // Dinsey Sound Source frequency 7100
        if (tick >= last_dss_tick + (1000000 / 7000)) {
            last_dss_sample = dss_sample();

            last_dss_tick = tick;
        }


        // Sound frequency 44100
        if (tick >= last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            int16_t samples[2] = { 0, 0 };
            OPL_calc_buffer_linear(emu8950_opl, (int32_t *)(samples), 1);

            if (last_dss_sample)
                samples[0] += last_dss_sample;
            if (speakerenabled)
                samples[0] += speaker_sample();

            samples[0] += sn76489_sample();


//            samples[0] += adlibgensample() * 32;

            samples[0] += covox_sample;

#if 1 || !PICO_ON_DEVICE
            if (last_sb_sample)
                samples[0] += last_sb_sample;
#endif

            samples[1] = samples[0];

            cms_samples(samples);


            audio_buffer[active_buffer][sample_index++] = samples[1];
            audio_buffer[active_buffer][sample_index++] = samples[0];


            if (sample_index >= AUDIO_BUFFER_LENGTH * 2) {
                sample_index = 0;
                i2s_dma_write(&i2s_config, audio_buffer[active_buffer]);
                active_buffer ^= 1;
            }

            last_sound_tick = tick;
        }
#endif
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

#if PICO_RP2350
void __no_inline_not_in_flash_func(psram_init)(uint cs_pin) {
    gpio_set_function(cs_pin, GPIO_FUNC_XIP_CS1);

    // Enable direct mode, PSRAM CS, clkdiv of 10.
    qmi_hw->direct_csr = 10 << QMI_DIRECT_CSR_CLKDIV_LSB | \
                        QMI_DIRECT_CSR_EN_BITS | \
                        QMI_DIRECT_CSR_AUTO_CS1N_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;

    // Enable QPI mode on the PSRAM
    const uint CMD_QPI_EN = 0x35;
    qmi_hw->direct_tx = QMI_DIRECT_TX_NOPUSH_BITS | CMD_QPI_EN;

    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;

    // Set PSRAM timing for APS6404
    //
    // Using an rxdelay equal to the divisor isn't enough when running the APS6404 close to 133MHz.
    // So: don't allow running at divisor 1 above 100MHz (because delay of 2 would be too late),
    // and add an extra 1 to the rxdelay if the divided clock is > 100MHz (i.e. sys clock > 200MHz).
    const int max_psram_freq = 166000000;
    const int clock_hz = clock_get_hz(clk_sys);
    int divisor = (clock_hz + max_psram_freq - 1) / max_psram_freq;
    if (divisor == 1 && clock_hz > 100000000) {
        divisor = 2;
    }
    int rxdelay = divisor;
    if (clock_hz / divisor > 100000000) {
        rxdelay += 1;
    }

    // - Max select must be <= 8us.  The value is given in multiples of 64 system clocks.
    // - Min deselect must be >= 18ns.  The value is given in system clock cycles - ceil(divisor / 2).
    const int clock_period_fs = 1000000000000000ll / clock_hz;
    const int max_select = (125 * 1000000) / clock_period_fs;  // 125 = 8000ns / 64
    const int min_deselect = (18 * 1000000 + (clock_period_fs - 1)) / clock_period_fs - (divisor + 1) / 2;

    qmi_hw->m[1].timing = 1 << QMI_M1_TIMING_COOLDOWN_LSB |
                          QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
                          max_select << QMI_M1_TIMING_MAX_SELECT_LSB |
                          min_deselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
                          rxdelay << QMI_M1_TIMING_RXDELAY_LSB |
                          divisor << QMI_M1_TIMING_CLKDIV_LSB;

    // Set PSRAM commands and formats
    qmi_hw->m[1].rfmt =
            QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB |\
        QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q   << QMI_M0_RFMT_ADDR_WIDTH_LSB |\
        QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB |\
        QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q  << QMI_M0_RFMT_DUMMY_WIDTH_LSB |\
        QMI_M0_RFMT_DATA_WIDTH_VALUE_Q   << QMI_M0_RFMT_DATA_WIDTH_LSB |\
        QMI_M0_RFMT_PREFIX_LEN_VALUE_8   << QMI_M0_RFMT_PREFIX_LEN_LSB |\
        6                                << QMI_M0_RFMT_DUMMY_LEN_LSB;

    qmi_hw->m[1].rcmd = 0xEB;

    qmi_hw->m[1].wfmt =
            QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB |\
        QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q   << QMI_M0_WFMT_ADDR_WIDTH_LSB |\
        QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB |\
        QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q  << QMI_M0_WFMT_DUMMY_WIDTH_LSB |\
        QMI_M0_WFMT_DATA_WIDTH_VALUE_Q   << QMI_M0_WFMT_DATA_WIDTH_LSB |\
        QMI_M0_WFMT_PREFIX_LEN_VALUE_8   << QMI_M0_WFMT_PREFIX_LEN_LSB;

    qmi_hw->m[1].wcmd = 0x38;

    // Disable direct mode
    qmi_hw->direct_csr = 0;

    // Enable writes to PSRAM
    hw_set_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_WRITABLE_M1_BITS);
}
#endif

int main() {

#if PICO_RP2350
    volatile uint32_t *qmi_m0_timing=(uint32_t *)0x400d000c;
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_50);
    sleep_ms(33);
    *qmi_m0_timing = 0x60007204;
    set_sys_clock_hz(444000000, 0);
    *qmi_m0_timing = 0x60007303;
#if !I2S_SOUND
    init_74hc595();
#endif
    psram_init(19);
#else
    memcpy_wrapper_replace(NULL);
    //vreg_set_voltage(VREG_VOLTAGE_1_30);
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    vreg_disable_voltage_limit();
    sleep_ms(33);
    set_sys_clock_khz(378 * 1000, true);

    if (!init_psram()) {
        printf("No PSRAM detected.");
    }
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

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);

    sleep_ms(50);

    graphics_set_mode(TEXTMODE_80x25_COLOR);

    if (FR_OK != f_mount(&fs, "0", 1)) {
        printf("SD Card not inserted or SD Card error!");
        while (1);
    }

    sn76489_reset();
    reset86();

    while (true) {
        exec86(32768);
        tight_loop_contents();
    }
    __unreachable();

}