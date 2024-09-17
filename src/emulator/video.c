#include "emulator.h"


uint8_t VIDEORAM[VIDEORAM_SIZE];

int videomode = 0;

void videoram_write(uint32_t address, uint8_t value) {
    if (log_debug) {
        printf(">> %x a %x\n", (address - 0xA0000) % VIDEORAM_SIZE, address);
    }
    VIDEORAM[(vga_plane_offset + address - 0xA0000) % VIDEORAM_SIZE] = value;
}

uint8_t videoram_read(uint32_t address) {
    return VIDEORAM[(vga_plane_offset + address - 0xA0000) % VIDEORAM_SIZE];
}
