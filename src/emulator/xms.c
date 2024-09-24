#include "emulator.h"

uint8_t xms_handler() {
    printf("XMS: %04X:%04X\n", CPU_AH, CPU_DX);
    switch (CPU_AH) {
        case 0x00: { // Get XMS Version
            CPU_AX = 0x0200;
            CPU_BX = 0x0400; // driver version
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
            CPU_AX = 0; // Success
            break;
        }
        case 0x03: { // Global Enable A20
            // Stub: Implement A20 enabling functionality
            CPU_AX = 0; // Success
            break;
        }
        case 0x04: { // Global Disable A20
            // Stub: Implement A20 disabling functionality
            CPU_AX = 0; // Success
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
            CPU_AX = 1; // Success
            break;
        }
        case 0x07: { // Query free extended memory
            // Stub: Implement querying free extended memory functionality
            CPU_AX = 0x0000; // Least significant word (free memory in KB)
            break;
        }
        case 0x08: { // Query largest free extended memory block
            // Stub: Implement querying largest free extended memory block functionality
            CPU_DX = 0x0000; // Most significant word (size of largest block in KB)
            CPU_AX = 0x0000; // Least significant word (size of largest block in KB)
            break;
        }
        case 0x09: { // Allocate extended memory block
            // Stub: Implement allocating extended memory block functionality
            CPU_DX = 0xFFFF; // Handle of allocated memory block
            CPU_AX = 0; // Success
            break;
        }
        case 0x0A: { // Free extended memory block
            // Stub: Implement freeing extended memory block functionality
            CPU_AX = 0; // Success
            break;
        }
        case 0x0B: { // Move extended memory block
            // Stub: Implement moving extended memory block functionality
            CPU_AX = 0; // Success
            break;
        }
        case 0x0C: { // Lock extended memory block
            // Stub: Implement locking extended memory block functionality4
            CPU_AX = 0; // Success
            break;
        }
        case 0x0D: { // Unlock extended memory block
            // Stub: Implement unlocking extended memory block functionality
            CPU_AX = 0; // Success
            break;
        }
        case 0x0E: { // Query XMS driver capabilities
            // Stub: Implement querying XMS driver capabilities functionality
            CPU_DX = 0x0000; // Capabilities
            CPU_AX = 0; // Success
            break;
        }
        case 0x0F: { // Query any free extended memory block
            // Stub: Implement querying any free extended memory block functionality
            CPU_DX = 0x0000; // Most significant word (size of any free block in KB)
            CPU_AX = 0x0000; // Least significant word (size of any free block in KB)
            break;
        }
        case 0x10: { // Request Upper Memory Block (Function 10h):
            static int i = 0;
            if (i) {
                CPU_DX = 0x0000;
                CPU_AX = 0x0000; // Success
                CPU_BX = 0x00B1; // Success
                break;
            }
            if (CPU_DX == 0xFFFF) {
                CPU_DX = 0x0800;
                CPU_AX = 0x1000; // Success
                CPU_BX = 0x00B0; // Success
            } else {

                i = 1;
                CPU_DX = 0x0800;
                CPU_BX = 0xC000;
                CPU_AX = 0x0001; // Success
            }

            break;
        }
        case 0x11: { // Query DMA buffer information
            // Stub: Implement querying DMA buffer information functionality
            CPU_DX = 0x0000; // Most significant word (DMA buffer size)
            CPU_AX = 0x0000; // Least significant word (DMA buffer size)
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