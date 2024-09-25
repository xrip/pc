#pragma GCC optimize("Ofast")
#include "includes/bios.h"
#include "emulator.h"

#if PICO_ON_DEVICE
#include "psram_spi.h"
#endif

uint8_t VIDEORAM[VIDEORAM_SIZE+1];
uint8_t RAM[RAM_SIZE+1];

extern uint8_t UMB[];
uint8_t HMA[65535];

void write86(uint32_t address, uint8_t value) {
    if (address < RAM_SIZE) {
        RAM[address] = value;
    }
#if PICO_ON_DEVICE
    else if (address < (640 << 10)) {
        write8psram(address, value);
        return;
    }
#endif
    else if (address >= 0xA0000 && address < 0xC0000) {
            VIDEORAM[(vga_plane_offset + address - 0xA0000) % VIDEORAM_SIZE] = value;
    } else if (address >= 0xC0000 && address < 0xFC000) {
        UMB[address - 0xC0000] = value;
        //printf(" >> Write86:  %04X, %x\n", address, value);
    } else if (address >= 0x100000 && address < 0x110000) {
        HMA[address - 0x100000] = value;
    }

}

// Write uint16_t to virutal RAM
void writew86(uint32_t address, uint16_t value) {
//    printf("W>> WriteW86: %04X, %02X\n", address, value);

// not 16 bit aligned
    if (address & 1) {
        write86(address, (uint8_t) value & 0xFF);
        write86(address + 1, (uint8_t) ((value >> 8) & 0xFF));
    } else {
        if (address >= 0xA0000 && address < 0xC0000) {
            *(uint16_t *)&VIDEORAM[(vga_plane_offset + address - 0xA0000) % VIDEORAM_SIZE] = value;
        } else if (address < RAM_SIZE) {
            *(uint16_t *)&RAM[address] = value;
        } else if (address >= 0xC0000 && address < 0xFC000) {
            *(uint16_t *) &UMB[address - 0xC0000] = value;
//            printf(" >> Writew86:  %04X, %x\n", address, value);
        } else if (address >= 0x100000 && address < 0x110000) {
            *(uint16_t *) &HMA[address - 0x100000] = value;
        }
    }
}


uint8_t read86(uint32_t address) {
    if (address < RAM_SIZE) {
        return RAM[address];
    }
#if PICO_ON_DEVICE
    else if (address < (640 << 10)) {
        return read8psram(address);
    }
#endif
    else if (address >= 0xA0000 && address < 0xC0000) {
        return VIDEORAM[(vga_plane_offset + address - 0xA0000) % VIDEORAM_SIZE];
    } else if (address >= 0xC0000 && address < 0xFC000) {
        //printf(" >> Read86:  %04X %x\n", address, TEMP[address - 0xC0000]);
        return UMB[address - 0xC0000];
    } else if (address == 0xFC000) { return 0x21; }
    else if (address >= 0xFE000 && address < 0x100000) {
        return BIOS[address - 0xFE000];
    } else if (address >= 0x100000 && address < 0x110000) {
        return HMA[address - 0x100000];
    }
    return 0;
}

uint16_t readw86(uint32_t address) {
    // not 16 bit aligned
    if (address & 1) {
        return (uint16_t) read86(address) | ((uint16_t) read86(address + 1) << 8);
    } else {
        if (address < RAM_SIZE) {
            return *(uint16_t *)&RAM[address];
        } else if (address >= 0xA0000 && address < 0xC0000) {
            return *(uint16_t *)&VIDEORAM[(vga_plane_offset + address - 0xA0000) % VIDEORAM_SIZE];
        } else if (address >= 0xC0000 && address < 0xFC000) {
            //printf(" >> Readw86:  %04X %x\n", address, *(uint16_t *)&TEMP[address - 0xC0000]);
            return *(uint16_t *) &UMB[address - 0xC0000];
        } else if (address >= 0xFE000 && address < 0x100000) {
            return *(uint16_t *)&BIOS[address - 0xFE000];
        } else if (address >= 0x100000 && address < 0x110000) {
            return *(uint16_t *) &HMA[address - 0x100000];
        }
        return 0;
    }
}
