// https://archive.org/details/dss-programmers-guide
#pragma GCC optimize("Ofast")
#include "emulator/emulator.h"

#define FIFO_BUFFER_SIZE 16

static uint8_t fifo_buffer[FIFO_BUFFER_SIZE] = { 0 };
static uint8_t fifo_length = 0;
static uint8_t dss_data;

int16_t covox_sample = 0;
inline uint16_t dss_sample() {
    register uint8_t sample = 0;
    if (fifo_length) {
        sample = *fifo_buffer;
        memmove(fifo_buffer, fifo_buffer + 1, --fifo_length);
    }
    return sample ? (int16_t )((sample-128) << 6) : 0;
}

inline static void fifo_push_byte(uint8_t value) { // core #0
    if (fifo_length == FIFO_BUFFER_SIZE)
        return;
    fifo_buffer[fifo_length++] = value;
}

static inline uint8_t fifo_is_full() {
    return fifo_length == FIFO_BUFFER_SIZE ? 0x40 : 0x00;
}

static INLINE uint8_t dss_in(uint16_t portnum) {
    return portnum & 1 ? fifo_is_full() : dss_data;
}

static INLINE void dss_out(uint16_t portnum, uint8_t value) {
    static uint8_t control;
    switch (portnum) {
        case 0x378:
            fifo_push_byte(value);
            dss_data = value;
            break;
        case 0x37A:
            if ((value & 4) && !(control & 4)){
                fifo_push_byte(dss_data);
            }
            control = value;
            break;
    }
}
