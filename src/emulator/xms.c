#include <stdbool.h>
#include "emulator.h"

// https://www.phatcode.net/res/219/files/xms20.txt
typedef struct umb {
    uint16_t segment;
    uint16_t size; // paragraphs
    bool allocated;
} umb_t;


#define UMB_BLOCKS_COUNT 6
static umb_t umb_blocks[UMB_BLOCKS_COUNT] = {
        0xC000, 0x0800, false,
        0xC800, 0x0800, false,
        0xE000, 0x0800, false,
        0xE800, 0x0800, false,
        0xF000, 0x0800, false,
        0xF800, 0x0400, false,
};
uint8_t UMB[0xFC000 - 0xC000] = { 0 };

static int umb_blocks_allocated = 0;


umb_t *get_free_umb_block(uint16_t size) {
    for (int i = umb_blocks_allocated; i < UMB_BLOCKS_COUNT; i++) {

        if (umb_blocks[i].allocated == false && umb_blocks[i].size <= size) {
            return &umb_blocks[i];
        }
    }
    return NULL;
}

uint8_t xms_handler() {
    printf("XMS: %04X:%04X\n", CPU_AH, CPU_DX);
    switch (CPU_AH) {
        case 0x00: { // Get XMS Version
            CPU_AX = 0x0100;
            CPU_BX = 0x0001; // driver version
            CPU_DX = 0x0001; // HMA Exist
            break;
        }
        case 0x01: { // Request HMA
            // Stub: Implement HMA request functionality
            CPU_AX = 1; // Success
            break;
        }
        case 0x02: { // Release HMA
            // Stub: Implement HMA release functionality
            CPU_AX = 1; // Success
            break;
        }
        case 0x03: { // Global Enable A20
            // Stub: Implement A20 enabling functionality
            break;
        }
        case 0x04: { // Global Disable A20
            // Stub: Implement A20 disabling functionality
            break;
        }
        case 0x05: { // Local Enable A20
            // Stub: Implement local A20 enabling functionality
            CPU_AX = 0x0001; // Success
            CPU_BL = 0x00;
            break;
        }
        case 0x06: { // Local Disable A20
            // Stub: Implement local A20 disabling functionality
            CPU_AX = 0; // Success
            CPU_BL = 0x94;
            break;
        }
        case 0x07: { // Query A20 (Function 07h):
            CPU_AX = 1; // Success
            // Stub: Implement querying free extended memory functionality
            break;
        }
        case 0x08: { // Query largest free extended memory block
            // Stub: Implement querying largest free extended memory block functionality
            break;
        }
        case 0x09: { // Allocate Extended Memory Block (Function 09h):
            // Stub: Implement allocating extended memory block functionality
            CPU_DX = 0x0000;
            CPU_AX = 0x0000;
            CPU_BL = 0xA0;
            break;
        }
        case 0x0A: { // Free extended memory block
            // Stub: Implement freeing extended memory block functionality
            break;
        }
        case 0x0B: { // Move extended memory block
            // Stub: Implement moving extended memory block functionality
            break;
        }
        case 0x0C: { // Lock extended memory block
            // Stub: Implement locking extended memory block functionality4
            break;
        }
        case 0x0D: { // Unlock extended memory block
            // Stub: Implement unlocking extended memory block functionality
            break;
        }
        case 0x0E: { // Get EMB Handle Information (Function 0Eh):
            // Stub: Implement querying XMS driver capabilities functionality
            break;
        }
        case 0x0F: { // Reallocate Extended Memory Block (Function 0Fh):
            // Stub: Implement querying any free extended memory block functionality
            break;
        }
        case 0x10: { // Request Upper Memory Block (Function 10h):
            if (CPU_DX == 0xFFFF) {
                if (umb_blocks_allocated < UMB_BLOCKS_COUNT) {
                    umb_t *umb_block = get_free_umb_block(CPU_DX);
                    if (umb_block != NULL) {
                        CPU_DX = umb_block->size;
                        CPU_AX = 0x1000; // Success
                        CPU_BX = 0x00B0; // Success
                        break;
                    }
                }
            } else {
                umb_t *umb_block = get_free_umb_block(CPU_DX);
                if (umb_block != NULL) {
                    CPU_BX = umb_block->segment;
                    CPU_DX = umb_block->size;
                    CPU_AX = 0x0001;

                    umb_block->allocated = true;
                    umb_blocks_allocated++;
                    break;
                }
            }

            CPU_AX = 0x0000;
            CPU_DX = 0x0000;
            CPU_BL = umb_blocks_allocated >= UMB_BLOCKS_COUNT ? 0xB1 : 0xB0;
            break;

        }
        case 0x11: { // Query DMA buffer information
            // Stub: Implement querying DMA buffer information functionality
            CPU_DX = 0x0000; // Most significant word (DMA buffer size)
            CPU_AX = 0x0001; // Least significant word (DMA buffer size)
            break;
        }
        default: {
            // Unhandled function
            CPU_AX = 0x0001; // Function not supported
            CPU_BL = 0x80; // Function not implemented
            break;
        }
    }
    return 0xCB;
}