#include <hardware/clocks.h>
#include "hardware/pio.h"
#include <hardware/pwm.h>
#include <hardware/timer.h>

#define PIO_74HC595 pio1
#define SM_74HC595 1

#define SHIFT_SPEED (15 * MHZ)

#define CLK_LATCH_595_BASE_PIN (26)
#define DATA_595_PIN (28)

#if PICO_RP2350
#define CLOCK_PIN 20
#define CLOCK_PIN2 21
#else
#define CLOCK_PIN 23
#define CLOCK_PIN2 29

#endif
#define CLOCK_FREQUENCY (3579545 * 2)
#define CLOCK_FREQUENCY2 (3579545 * 1)
//(14'318'180)


#define A0 (1 << 8)
#define A1 (1 << 9)

#define IC (1 << 10)

#define SN_1_CS (1 << 11)

#define SAA_1_CS (1 << 12)
#define SAA_2_CS (1 << 13)

#define OPL2 (1 << 14)
#define OPL3 (1 << 15)

static uint16_t control_bits = 0;
#define LOW(x) (control_bits &= ~(x))
#define HIGH(x) (control_bits |= (x))

void SN76489_write(uint8_t byte);
void SAA1099_write(uint8_t addr, uint8_t chip, uint8_t byte);
void OPL2_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte);

static const uint16_t program_instructions595[] = {
        //     .wrap_target
        0x80a0, //  0: pull   block           side 0
        0x6001, //  1: out    pins, 1         side 0
        0x1041, //  2: jmp    x--, 1          side 2
        0xe82f, //  3: set    x, 15           side 1
        0x6050, //  4: out    y, 16           side 0
        0x0085, //  5: jmp    y--, 5          side 0
        //     .wrap

};

static const struct pio_program program595 = {
        .instructions = program_instructions595,
        .length = 6,
        .origin = -1,
};


static void clock_init(uint pin, uint32_t frequency) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config c_pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&c_pwm, clock_get_hz(clk_sys) / (4.0 * frequency));
    pwm_config_set_wrap(&c_pwm, 3); // MAX PWM value
    pwm_init(slice_num, &c_pwm, true);
    pwm_set_gpio_level(pin, 2);
}
void static inline reset_chips();
static inline void init_74hc595() {
    clock_init(CLOCK_PIN, CLOCK_FREQUENCY);
    clock_init(CLOCK_PIN2, CLOCK_FREQUENCY2);
    uint offset = pio_add_program(PIO_74HC595, &program595);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + (program595.length - 1));
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    //настройка side set
    sm_config_set_sideset_pins(&c, CLK_LATCH_595_BASE_PIN);
    sm_config_set_sideset(&c, 2, false, false);

    for (int i = 0; i < 2; i++) {
        pio_gpio_init(PIO_74HC595, CLK_LATCH_595_BASE_PIN + i);
    }

    pio_sm_set_pins_with_mask(PIO_74HC595, SM_74HC595, 3u << CLK_LATCH_595_BASE_PIN, 3u << CLK_LATCH_595_BASE_PIN);
    pio_sm_set_pindirs_with_mask(PIO_74HC595, SM_74HC595, 3u << CLK_LATCH_595_BASE_PIN, 3u << CLK_LATCH_595_BASE_PIN);
    //

    pio_gpio_init(PIO_74HC595, DATA_595_PIN);//резервируем под выход PIO

    pio_sm_set_consecutive_pindirs(PIO_74HC595, SM_74HC595, DATA_595_PIN, 1, true);//конфигурация пинов на выход

    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_out_pins(&c, DATA_595_PIN, 1);

    pio_sm_init(PIO_74HC595, SM_74HC595, offset, &c);
    pio_sm_set_enabled(PIO_74HC595, SM_74HC595, true);

    pio_sm_set_clkdiv(PIO_74HC595, SM_74HC595, clock_get_hz(clk_sys) / SHIFT_SPEED);

    // Reset PIO program
    PIO_74HC595->txf[SM_74HC595] = 0;
    reset_chips();
}

static inline void write_74hc595(uint16_t data, uint16_t delay_us) {
    PIO_74HC595->txf[SM_74HC595] = data << 16 | delay_us << 4; // 1 microsecond per 15 cycles @ 15Mhz
}

void static inline reset_chips() {
    control_bits = 0;
    write_74hc595(HIGH(SN_1_CS | OPL2 | SAA_1_CS | SAA_2_CS | OPL3), 0);
    write_74hc595(HIGH(IC), 0);
    busy_wait_ms(10);
    write_74hc595(LOW(IC), 0);
    busy_wait_ms(100);
    write_74hc595(HIGH(IC), 0);
    busy_wait_ms(10);

    // Mute SN76489
    SN76489_write(0x9F);
    busy_wait_ms(10);
    SN76489_write(0xBF);
    busy_wait_ms(10);
    SN76489_write(0xDF);
    busy_wait_ms(10);
    SN76489_write(0xFF);
    busy_wait_ms(10);

    for (int i = 0; i < 6; i++) {
        busy_wait_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        busy_wait_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
}
