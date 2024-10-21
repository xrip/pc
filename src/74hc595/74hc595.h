#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "hardware/pio.h"

#define PIO_74HC595 pio1
#define SM_74HC595 1

// 10Mhz
#define SHIFT_SPEED (10*1500000)

#define CLK_LATCH_595_BASE_PIN (26)
#define DATA_595_PIN (28)

void init_74hc595();
void write_74hc595(uint16_t data);
void saa1099_write(uint8_t chip, uint8_t addr, uint8_t byte);
void sn76489_write(uint8_t byte);
void ymf262_write_byte(uint8_t addr, uint8_t register_set, uint8_t byte);

#ifdef __cplusplus
}
#endif