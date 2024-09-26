#pragma GCC optimize("Ofast")

#include "includes/bios.h"
#include "emulator.h"

const size_t VIDEORAM_LIMIT = 0xA0000;
const size_t VIDEORAM_END = 0xC0000;
const size_t UMB_START = 0xC0000;
const size_t UMB_END = 0xFC000;
const size_t HMA_START = 0x100000;
const size_t HMA_END = 0x110000;
const size_t BIOS_START = 0xFE000;

uint8_t VIDEORAM[VIDEORAM_SIZE + 1];
uint8_t RAM[RAM_SIZE + 1];
extern uint8_t UMB[];
uint8_t HMA[0xFFFF];

// Writes a byte to the virtual memory
void write86(uint32_t address, uint8_t value) {
    if (address < RAM_SIZE) {
        RAM[address] = value;
    } else if (address >= VIDEORAM_LIMIT && address < VIDEORAM_END) {
        if (log_debug)
            printf("Writing %04X %02x\n", (vga_plane_offset + address - VIDEORAM_LIMIT) % VIDEORAM_SIZE, value);
        VIDEORAM[(vga_plane_offset + address - VIDEORAM_LIMIT) % VIDEORAM_SIZE] = value;
    } else if (address >= UMB_START && address < UMB_END) {
        UMB[address - UMB_START] = value;
    } else if (address >= HMA_START && address < HMA_END) {
        HMA[address - HMA_START] = value;
    }
}

// Writes a word to the virtual memory
void writew86(uint32_t address, uint16_t value) {
    if (address & 1) {
        write86(address, (uint8_t) (value & 0xFF));
        write86(address + 1, (uint8_t) ((value >> 8) & 0xFF));
    } else {
        if (address < RAM_SIZE) {
            *(uint16_t *) &RAM[address] = value;
        } else if (address >= VIDEORAM_LIMIT && address < VIDEORAM_END) {
            if (log_debug)
                printf("WritingW %04X %04x\n", (vga_plane_offset + address - VIDEORAM_LIMIT) % VIDEORAM_SIZE, value);
            *(uint16_t *) &VIDEORAM[(vga_plane_offset + address - VIDEORAM_LIMIT) % VIDEORAM_SIZE] = value;
        } else if (address >= UMB_START && address < UMB_END) {
            *(uint16_t *) &UMB[address - UMB_START] = value;
        } else if (address >= HMA_START && address < HMA_END) {
            *(uint16_t *) &HMA[address - HMA_START] = value;
        }
    }
}

// Reads a byte from the virtual memory
uint8_t read86(uint32_t address) {
    if (address < RAM_SIZE) {
        return RAM[address];
    } else if (address >= VIDEORAM_LIMIT && address < VIDEORAM_END) {
        return VIDEORAM[(vga_plane_offset + address - VIDEORAM_LIMIT) % VIDEORAM_SIZE];
    } else if (address >= UMB_START && address < UMB_END) {
        return UMB[address - UMB_START];
    } else if (address == 0xFC000) {
        return 0x21;
    } else if (address >= BIOS_START && address < HMA_START) {
        return BIOS[address - BIOS_START];
    } else if (address >= HMA_START && address < HMA_END) {
        return HMA[address - HMA_START];
    }
    return 0;
}

// Reads a word from the virtual memory
uint16_t readw86(uint32_t address) {
    if (address & 1) {
        return (uint16_t) read86(address) | ((uint16_t) read86(address + 1) << 8);
    } else {
        if (address < RAM_SIZE) {
            return *(uint16_t *) &RAM[address];
        } else if (address >= VIDEORAM_LIMIT && address < VIDEORAM_END) {
            return *(uint16_t *) &VIDEORAM[(vga_plane_offset + address - VIDEORAM_LIMIT) % VIDEORAM_SIZE];
        } else if (address >= UMB_START && address < UMB_END) {
            return *(uint16_t *) &UMB[address - UMB_START];
        } else if (address >= BIOS_START && address < HMA_START) {
            return *(uint16_t *) &BIOS[address - BIOS_START];
        } else if (address >= HMA_START && address < HMA_END) {
            return *(uint16_t *) &HMA[address - HMA_START];
        }
        return 0;
    }
}