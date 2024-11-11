#include "74hc595.h"
#include "pico/platform.h"
#define SHIFT_SPEED (32 * MHZ)
static volatile uint16_t control_bits = 0;
#define LOW(x) (control_bits &= ~(x))
#define HIGH(x) (control_bits |= (x))

#if SN76489_REVERSED
// Если мы перепутаем пины
static const uint8_t  __aligned(4) reversed[] = {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};
#endif

static int SM_74HC595 = -1;
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

static int SM_CLOCK = -1;

static const uint16_t clock_program_instructions[] = {
        //     .wrap_target
        0xa042, //  0: nop                    side 0
        0x1000, //  1: jmp    0               side 1
        //     .wrap
};

static const struct pio_program clock_program = {
        .instructions = clock_program_instructions,
        .length = 2,
        .origin = -1,
};


void clock_init(uint pin, uint32_t frequency) {
    if (-1 == SM_CLOCK) {
        SM_CLOCK = pio_claim_unused_sm(PIO_CLOCK, true);

        uint offset = pio_add_program(PIO_CLOCK, &clock_program);

        pio_sm_config c = pio_get_default_sm_config();
        sm_config_set_wrap(&c, offset, offset + 1);
        sm_config_set_sideset(&c, 1, false, false);

        // Map the state machine's OUT pin group to one pin, namely the `pin`
        // parameter to this function.
        sm_config_set_out_pins(&c, pin, 1);
        // Set this pin's GPIO function (connect PIO to the pad)
        pio_gpio_init(PIO_CLOCK, pin);

        sm_config_set_sideset_pins(&c, pin);
        // Set the pin direction to output at the PIO
        pio_sm_set_consecutive_pindirs(PIO_CLOCK, SM_CLOCK, pin, 1, true);
        // Load our configuration, and jump to the start of the program
        pio_sm_init(PIO_CLOCK, SM_CLOCK, offset, &c);

        pio_sm_set_clkdiv(PIO_CLOCK, SM_CLOCK, clock_get_hz(clk_sys) / (frequency * 2));
        // Set the state machine running
        pio_sm_set_enabled(PIO_CLOCK, SM_CLOCK, true);
    } else {
        pio_sm_set_clkdiv(PIO_CLOCK, SM_CLOCK, clock_get_hz(clk_sys) / (frequency * 2));
    }
}

void init_74hc595() {
    clock_init(CLOCK_PIN, CLOCK_FREQUENCY);

    uint offset = pio_add_program(PIO_74HC595, &program595);
    pio_sm_config c = pio_get_default_sm_config();

    SM_74HC595 = pio_claim_unused_sm(PIO_74HC595, true);
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

//    pio_sm_set_clkdiv(PIO_74HC595, SM_74HC595, (396.0f / 80.0f));
    pio_sm_set_clkdiv(PIO_74HC595, SM_74HC595, clock_get_hz(clk_sys) / (2 * SHIFT_SPEED));
    // Reset PIO program
    PIO_74HC595->txf[SM_74HC595] = 0;

    reset_chips();
}

static inline void write_74hc595(register uint16_t data, register uint16_t delay_us) {
    pio_sm_put_blocking(PIO_74HC595, SM_74HC595, (uint32_t)(data << 16 | delay_us << 6) ); // 1us @ 64Mhz
//    PIO_74HC595->txf[SM_74HC595] = data << 16 | (delay_us << 6); // 1 microsecond per 15 cycles @ 15Mhz
}

void reset_chips() {
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

    // Mute SAA1099
    SAA1099_write(1, 0, 0x1C); SAA1099_write(0, 0, 0);
    SAA1099_write(1, 1, 0x1C); SAA1099_write(0, 1, 0);
}


// SN76489
void SN76489_write(uint8_t byte) {
#if SN76489_REVERSED
    byte = reversed[byte];
#endif
    write_74hc595(byte | LOW(SN_1_CS), 10);
    write_74hc595(byte | HIGH(SN_1_CS), 0);
}

// YM2413
static inline void YM2413_write(uint8_t addr, uint8_t byte) {
    const uint16_t a0 = addr << 8;
    write_74hc595(byte | a0 | LOW(OPL2), 4);
    write_74hc595(byte | a0 | HIGH(OPL2), a0 ? 30 : 5);
}

// SAA1099
void SAA1099_write(uint8_t addr, uint8_t chip, uint8_t byte) {
    const uint16_t a0 = addr << 8;
    const uint16_t cs = chip ? SAA_2_CS : SAA_1_CS;

    write_74hc595(byte | a0 | LOW(cs), 5);
    write_74hc595(byte | a0 | HIGH(cs), 0);
}

// YM3812 / YM2413
void OPL2_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte) {
    const uint16_t a0 = addr << 8;
    const uint16_t a1 = register_set << 9;

    write_74hc595(byte | a0 | a1 | LOW(OPL2), 5);
    write_74hc595(byte | a0 | a1 | HIGH(OPL2), 30);
}

// YM3812 / YMF262
void OPL3_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte) {
    const uint16_t a0 = addr << 8;
    const uint16_t a1 = register_set << 9;

    write_74hc595(byte | a0 | a1 | LOW(OPL3), 5);
    write_74hc595(byte | a0 | a1 | HIGH(OPL3), 0);
}

