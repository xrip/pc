#include "bios.h"
#include "emulator.h"


uint8_t RAM[RAM_SIZE]; // +4 for safe WORD reads

void write86(uint32_t address, uint8_t value) {
    if (address >= 0xA0000 && address < 0xC0000) {
        videoram_write(address, value);
    } else if (address <= RAM_SIZE) {
        RAM[address] = value;
    }
//    printf(" >> Write86:  %04X, %x\n", address, value);
}

void writew86(uint32_t address, uint16_t value) {
//    printf("W>> WriteW86: %04X, %02X\n", address, value);
    write86(address, (uint8_t) value & 0xFF);
    write86(address + 1, (uint8_t) ((value >> 8) & 0xFF));
}


uint8_t read86(uint32_t address) {
    if (address == 0xFC000) { return 0x21; };
    if (address >= 0xA0000 && address <= 0xC0000) {
        return videoram_read(address);
    } else if (address >= 0xFE000) {
        return BIOS[address - 0xFE000];
    } else if (address <= RAM_SIZE) {
        return RAM[address];
    }
    return 0;
}

uint16_t readw86(uint32_t address) {
    return (uint16_t) read86(address) | ((uint16_t) read86(address + 1) << 8);
}
