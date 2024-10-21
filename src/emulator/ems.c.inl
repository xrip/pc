// The Lo-tech EMS board driver is hardcoded to 2MB.
static uint8_t ems_pages[4] = { 0 };

void out_ems(uint16_t port, uint8_t data) {
    ems_pages[port & 3] = data;
}

#if PICO_ON_DEVICE
uint8_t * PSRAM_DATA = (uint8_t*)0x11000000;

#define EMS_PSRAM_OFFSET (2048 << 10)
static INLINE void write8psram(uint32_t address, uint8_t value) {
    PSRAM_DATA[address] = value;
}
static INLINE void write16psram(uint32_t address, uint16_t value) {
    *(uint16_t *)&PSRAM_DATA[address] = value;
}

static INLINE uint8_t read8psram(uint32_t address) {
//    printf("PSRAM read 0x%8x %2x\n", address, PSRAM_DATA[address]);
    return PSRAM_DATA[address];
}
static INLINE uint16_t read16psram(uint32_t address) {

//    uint16_t result = (uint16_t) PSRAM_DATA[address] | ((uint16_t) PSRAM_DATA[address+1] << 8);
    uint16_t result = *(uint16_t *)&PSRAM_DATA[address];
//    (uint16_t) read86(address) | ((uint16_t) read86(address + 1) << 8);
//    printf("PSRAM read16 0x%8x %2x\n", address, result);
    return result;
}

static INLINE uint32_t physical_address(uint32_t address) {
    uint32_t page_addr = address & 0x3FFF;
    uint8_t selector = ems_pages[(address >> 14) & 3];
    return selector * 0x4000 + page_addr;
}
/*
static INLINE void write8psram(uint32_t address, uint8_t value) {
    printf("PSRAM write %x %x\n", address, value);
    register uint32_t *ptr = &PSRAM_DATA[address >> 2];   // Get the 32-bit word index
    register uint32_t data = *ptr;
    register uint32_t shift = (address & 0x3) * 8;        // Calculate byte offset (0, 8, 16, or 24 bits)
    register uint32_t mask = ~(0xFF << shift);            // Mask to clear the specific byte
    *ptr = (data & mask) | ((uint32_t)value << shift); // Clear and set the byte
}
static INLINE void write16psram(uint32_t address, uint16_t value) {
    printf("PSRAM write16 %x %x\n", address, value);
    uint32_t *ptr = &PSRAM_DATA[address >> 2];   // Get the 32-bit word index
    uint32_t shift = (address & 0x2) * 8;        // Calculate half-word offset (0 or 16 bits)

    uint32_t mask = ~(0xFFFF << shift);          // Mask to clear the specific half-word
    *ptr = (*ptr & mask) | ((uint32_t)value << shift); // Clear and set the half-word
}
static INLINE uint8_t read8psram(uint32_t address) {
    uint32_t *ptr = &PSRAM_DATA[address >> 2];   // Get the 32-bit word index
    uint32_t shift = (address & 0x3) * 8;        // Calculate byte offset (0, 8, 16, or 24 bits)
    uint8_t result = (*ptr >> shift) & 0xFF;
    printf("PSRAM read16 %x %x\n", address, result);
    return result;               // Extract the specific byte
}
static INLINE uint16_t read16psram(uint32_t address) {
    uint32_t *ptr = &PSRAM_DATA[address >> 2];   // Get the 32-bit word index
    uint32_t shift = (address & 0x2) * 8;        // Calculate half-word offset (0 or 16 bits)
    uint16_t result = (*ptr >> shift) & 0xFFFF;
    printf("PSRAM read16 %x %x\n", address, result);
    return result;             // Extract the specific half-word
}
*/
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
uint8_t ALIGN(4, EMS[EMS_MEMORY_SIZE + 4]) = {0 };

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