#include "emulator/emulator.h"

#define FIFO_BUFFER_SIZE 16
static uint8_t samples_buffer[FIFO_BUFFER_SIZE] = { 0 };
static uint8_t fifo_buffer_length = 0, dss_active = 0;
static uint8_t checks = 0;

uint8_t dss_sample() { // core #1
    if (fifo_buffer_length == 0 || !dss_active || checks < 2) { // no bytes in buffer
        return 0;
    }
    const uint8_t sample = samples_buffer[0];

    memmove(samples_buffer, samples_buffer + 1, FIFO_BUFFER_SIZE - 1);

    fifo_buffer_length--;
    return sample;
}

inline static void fifo_push_byte(uint8_t value) { // core #0
    if (fifo_buffer_length == FIFO_BUFFER_SIZE)
        return;
    samples_buffer[fifo_buffer_length++] = value;
//     printf("SS BUFF %i\r\n", value);
}

static inline uint8_t fifo_is_full() {
    return fifo_buffer_length == FIFO_BUFFER_SIZE ? 0x40 : 0x00;
}

void dss_out(uint16_t portnum, uint8_t value) {
    //printf("OUT SS %x %x\r\n", portnum, value);
    static uint8_t last37a, port378 = 0;
    switch (portnum) {
        case 0x378:
            port378 = value;
            fifo_push_byte(value);
            last37a = 0;
            break;
        case 0x37A:
            // Зачем слать предидущий байт в буфер если это не инит?
            if ((value & 4) && !(last37a & 4)) {
                //putssourcebyte(port378);
            }
            if (value == 0x04) {
                dss_active = 1;
            }
            last37a = value;
            break;
    }
}

uint8_t dss_in(uint16_t portnum) {
    if (checks < 2) checks++;
    return fifo_is_full();
}

void covox_out(uint16_t portnum, uint8_t value) {

}