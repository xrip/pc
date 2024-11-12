// The Lo-tech EMS board driver is hardcoded to 2MB.
#define EMS_PSRAM_OFFSET (2048 << 10)

static uint8_t ems_pages[4] = {0};

INLINE void out_ems(const uint16_t port, const uint8_t data) {
    ems_pages[port & 3] = data;
}

static INLINE uint32_t physical_address(const uint32_t address) {
    const uint32_t page_addr = address & 0x3FFF;
    const uint8_t selector = ems_pages[(address >> 14) & 3];
    return selector * 0x4000 + page_addr;
}

#if PICO_ON_DEVICE
static INLINE uint8_t ems_read(uint32_t addr) {
    return read8psram(EMS_PSRAM_OFFSET+physical_address(addr));
}
static INLINE uint16_t ems_readw(uint32_t addr) {
    return read16psram(EMS_PSRAM_OFFSET+physical_address(addr));
}
static INLINE void ems_write(uint32_t addr, uint8_t data) {
    write8psram(EMS_PSRAM_OFFSET+physical_address(addr), data);
}
static INLINE void ems_writew(uint32_t addr, uint16_t data) {
    write16psram(EMS_PSRAM_OFFSET+physical_address(addr), data);
}
#else
uint8_t ALIGN(4, EMS[EMS_MEMORY_SIZE + 4]) = {0};

static INLINE uint8_t ems_read(const uint32_t address) {
    const uint32_t phys_addr = physical_address(address);
    return EMS[phys_addr];
}

// TODO: Overlap?
static INLINE uint16_t ems_readw(const uint32_t addr) {
    const uint32_t phys_addr = physical_address(addr);
    return *(uint16_t *) &EMS[phys_addr];
}

static INLINE void ems_write(const uint32_t addr, const uint8_t data) {
    const uint32_t phys_addr = physical_address(addr);
    EMS[phys_addr] = data;
}


static INLINE void ems_writew(const uint32_t addr, const uint16_t data) {
    const uint32_t phys_addr = physical_address(addr);
    *(uint16_t *) &EMS[phys_addr] = data;
}

#endif