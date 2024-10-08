#pragma GCC optimize("O3")

#include "includes/bios.h"
#include "emulator.h"
#include "emulator/ems.c.inc"

#if PICO_ON_DEVICE
#include "psram_spi.h"
uint8_t RAM[RAM_SIZE + 4]   __attribute__((aligned(4))) = { 0 };
uint8_t VIDEORAM[VIDEORAM_SIZE + 4] __attribute__((aligned(4))) = { 0 };


// Writes a byte to the virtual memory
void __time_critical_func() write86(uint32_t address, uint8_t value) {
    if (address < RAM_SIZE) {
        RAM[address] = value;
    } else if (address < VIDEORAM_START) {
        write8psram(address, value);
    } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
        VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE] = value;
    } else if (address >= EMS_START && address < EMS_END) {
        ems_write(address - EMS_START, value);
    }  else if (address >= UMB_START && address < UMB_END) {
        write8psram(address, value);
    } else if (address >= HMA_START && address < HMA_END) {
        write8psram(address, value);
    }
}

// Writes a word to the virtual memory
void __time_critical_func() writew86(uint32_t address, uint16_t value) {
    if (address & 1) {
        write86(address, (uint8_t) (value & 0xFF));
        write86(address + 1, (uint8_t) ((value >> 8) & 0xFF));
    } else {
        if (address < RAM_SIZE) {
            *(uint16_t *) &RAM[address] = value;
        } else if (address < VIDEORAM_START) {
            write16psram(address, value);
        }  else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
            *(uint16_t *) &VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE] = value;
        } else if (address >= EMS_START && address < EMS_END) {
            ems_writew(address - EMS_START, value);
        }  else if (address >= UMB_START && address < UMB_END) {
            write16psram(address, value);
        } else if (address >= HMA_START && address < HMA_END) {
            write16psram(address, value);
        }
    }
}

// Reads a byte from the virtual memory
uint8_t __time_critical_func() read86(uint32_t address) {
    if (address < RAM_SIZE) {
        return RAM[address];
    } else if (address < VIDEORAM_START) {
        return read8psram(address);
    } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
        return VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE];
    } else if (address >= EMS_START && address < EMS_END) {
        return ems_read(address - EMS_START);
    } else if (address >= UMB_START && address < UMB_END) {
        return read8psram(address);
    } else if (address == 0xFC000) {
        return 0x21;
    } else if (address >= BIOS_START && address < HMA_START) {
        return BIOS[address - BIOS_START];
    } else if (address >= HMA_START && address < HMA_END) {
        return read8psram(address);
    }
    return 0xFF;
}

// Reads a word from the virtual memory
uint16_t __time_critical_func() readw86(uint32_t address) {
    if (address & 1) {
        return (uint16_t) read86(address) | ((uint16_t) read86(address + 1) << 8);
    } else {
        if (address < RAM_SIZE) {
            return *(uint16_t *) &RAM[address];
        }  else if (address < VIDEORAM_START) {
            return read16psram(address);
        } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
            return *(uint16_t *) &VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE];
        } else if (address >= EMS_START && address < EMS_END) {
            return ems_readw(address - EMS_START);
        } else if (address >= UMB_START && address < UMB_END) {
            return read16psram(address);
        } else if (address >= BIOS_START && address < HMA_START) {
            return *(uint16_t *) &BIOS[address - BIOS_START];
        } else if (address >= HMA_START && address < HMA_END) {
            return read16psram(address);
        }
    }
    return 0xFFFF;
}
#else
uint8_t VIDEORAM[VIDEORAM_SIZE + 4]  __attribute__((aligned(4))) = { 0 };
uint8_t RAM[RAM_SIZE + 4]  __attribute__((aligned(4))) = { 0 };
uint8_t UMB[(UMB_END - UMB_START) + 4]  __attribute__((aligned(4))) = { 0 };
uint8_t HMA[(HMA_END - HMA_START) + 4]  __attribute__((aligned(4))) = { 0 };

// Writes a byte to the virtual memory
void write86(uint32_t address, uint8_t value) {
    if (address < RAM_SIZE) {
        RAM[address] = value;
    } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
        if (log_debug)
            printf("Writing %04X %02x\n", (vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE, value);
        VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE] = value;
    } else if (address >= EMS_START && address < EMS_END) {
        ems_write(address - EMS_START, value);
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
        } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
            if (log_debug)
                printf("WritingW %04X %04x\n", (vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE, value);
            *(uint16_t *) &VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE] = value;
        } else if (address >= EMS_START && address < EMS_END) {
            ems_writew(address - EMS_START, value);
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
    } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
        return VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE];
    } else if (address >= EMS_START && address < EMS_END) {
        return ems_read(address - EMS_START);
    }  else if (address >= UMB_START && address < UMB_END) {
        return UMB[address - UMB_START];
    }else if (address == 0xFC000) {
        return 0x21;
    } else if (address >= BIOS_START && address < HMA_START) {
        return BIOS[address - BIOS_START];
    } else if (address >= HMA_START && address < HMA_END) {
        return HMA[address - HMA_START];
    }
    return 0xFF;
}

// Reads a word from the virtual memory
uint16_t readw86(uint32_t address) {
    if (address & 1) {
        return (uint16_t) read86(address) | ((uint16_t) read86(address + 1) << 8);
    } else {
        if (address < RAM_SIZE) {
            return *(uint16_t *) &RAM[address];
        } else if (address >= VIDEORAM_START && address < VIDEORAM_END) {
            return *(uint16_t *) &VIDEORAM[(vga_plane_offset + address - VIDEORAM_START) % VIDEORAM_SIZE];
        }  else if (address >= EMS_START && address < EMS_END) {
            return ems_readw(address - EMS_START);
        }else if (address >= UMB_START && address < UMB_END) {
            return *(uint16_t *) &UMB[address - UMB_START];
        } else if (address >= BIOS_START && address < HMA_START) {
            return *(uint16_t *) &BIOS[address - BIOS_START];
        } else if (address >= HMA_START && address < HMA_END) {
            return *(uint16_t *) &HMA[address - HMA_START];
        }
    }
    return 0xFFFF;
}

#endif