#pragma once
#include <hardware/clocks.h>
#include "hardware/pio.h"
#include <hardware/pwm.h>
#include <hardware/timer.h>

#define PIO_74HC595 pio0

#define CLK_LATCH_595_BASE_PIN (26)
#define DATA_595_PIN (28)


#define CLOCK_PIN 17
#define CLOCK_FREQUENCY (3579545)

//(14'318'180)


#define A0 (1 << 8)
#define A1 (1 << 9)

#define IC (1 << 10)

#define SN_1_CS (1 << 11)

#define SAA_1_CS (1 << 12)
#define SAA_2_CS (1 << 13)

#define OPL2 (1 << 14)
#define OPL3 (1 << 15)


void SN76489_write(uint8_t byte);
void SAA1099_write(uint8_t addr, uint8_t chip, uint8_t byte);
void OPL2_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte);
void OPL3_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte);
void reset_chips();
void init_74hc595();
void clock_init(uint pin, uint32_t frequency);


#define clock_wrap_target 0
#define clock_wrap 1

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

static inline pio_sm_config clock3_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + clock_wrap_target, offset + clock_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

static inline void clock3_program_init(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    pio_sm_config c = clock3_program_get_default_config(offset);
    // Map the state machine's OUT pin group to one pin, namely the `pin`
    // parameter to this function.
    sm_config_set_out_pins(&c, pin, 1);
    // Set this pin's GPIO function (connect PIO to the pad)
    pio_gpio_init(pio, pin);

    sm_config_set_sideset_pins(&c, pin);
    // Set the pin direction to output at the PIO
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    // Load our configuration, and jump to the start of the program
    pio_sm_init(pio, sm, offset, &c);

    pio_sm_set_clkdiv(pio, sm, clock_get_hz(clk_sys) / (freq * 2));
    // Set the state machine running
    pio_sm_set_enabled(pio, sm, true);
}

static inline void init_clock_pio(PIO pio, uint sm, uint pin, uint freq) {

    uint offset = pio_add_program(pio, &clock_program);
    clock3_program_init(pio,sm,offset,pin,freq);

}