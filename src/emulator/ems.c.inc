// The Lo-tech EMS board driver is hardcoded to 2MB.
static uint8_t ems_pages[4] = { 0 };

void out_ems(uint16_t port, uint8_t data) {
    ems_pages[port & 3] = data;
}

#if PICO_ON_DEVICE
#include "psram_spi.h"
#define EMS_PSRAM_OFFSET (2048 << 10)

static INLINE uint32_t physical_address(uint32_t address) {
    uint32_t page_addr = address & 0x3FFF;
    uint8_t selector = ems_pages[(address >> 14) & 3];
    return selector * 0x4000 + page_addr;
}

uint8_t ems_read(uint32_t addr) {
    uint32_t phys_addr = physical_address(addr);
    return (phys_addr < EMS_MEMORY_SIZE) ? read8psram(EMS_PSRAM_OFFSET+phys_addr) : 0xFF;
}

// TODO: Overlap?
uint16_t ems_readw(uint32_t addr) {
    uint32_t phys_addr = physical_address(addr);
    return (phys_addr < EMS_MEMORY_SIZE) ? read16psram(EMS_PSRAM_OFFSET+phys_addr) : 0xFFFF;
}

void ems_write(uint32_t addr, uint8_t data) {
    uint32_t phys_addr = physical_address(addr);
    if (phys_addr < EMS_MEMORY_SIZE)
        write8psram(EMS_PSRAM_OFFSET+phys_addr, data);
}


void ems_writew(uint32_t addr, uint16_t data) {
    uint32_t phys_addr = physical_address(addr);
    if (1 + phys_addr < EMS_MEMORY_SIZE)
        write16psram(EMS_PSRAM_OFFSET+phys_addr, data);
}
#else
uint8_t EMS[EMS_MEMORY_SIZE + 4]  __attribute__((aligned(4))) = { 0 };

static INLINE uint32_t physical_address(uint32_t address) {
    uint32_t page_addr = address & 0x3FFF;
    uint8_t selector = ems_pages[(address >> 14) & 3];
    return selector * 0x4000 + page_addr;
}

static INLINE uint8_t ems_read(uint32_t addr) {
    uint32_t phys_addr = physical_address(addr);
    return (phys_addr < EMS_MEMORY_SIZE) ? EMS[phys_addr] : 0xFF;
}

// TODO: Overlap?
static INLINE  uint16_t ems_readw(uint32_t addr) {
    uint32_t phys_addr = physical_address(addr);
    return (phys_addr < EMS_MEMORY_SIZE) ? *(uint16_t *) &EMS[phys_addr] : 0xFFFF;
}

static INLINE void ems_write(uint32_t addr, uint8_t data) {
    uint32_t phys_addr = physical_address(addr);
    if (phys_addr < EMS_MEMORY_SIZE)
        EMS[phys_addr] = data;
}


static INLINE void ems_writew(uint32_t addr, uint16_t data) {
    uint32_t phys_addr = physical_address(addr);
    if (1 + phys_addr < EMS_MEMORY_SIZE)
        *(uint16_t *) &EMS[phys_addr] = data;
}
#endif