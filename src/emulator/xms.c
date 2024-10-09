#include <stdbool.h>
#include "emulator.h"

// https://www.phatcode.net/res/218/files/limems40.txt
// https://www.phatcode.net/res/219/files/xms20.txt
// http://www.techhelpmanual.com/944-xms_functions.html
// http://www.techhelpmanual.com/651-emm_functions.html
// http://www.techhelpmanual.com/650-expanded_memory_specification__ems_.html
// http://www.techhelpmanual.com/943-extended_memory_specification__xms_.html
typedef struct umb {
    uint16_t segment;
    uint16_t size; // paragraphs
    bool allocated;
} umb_t;

static umb_t umb_blocks[] = {
// Used by EMS driver
//        { 0xC000, 0x0800, false },
//        { 0xC800, 0x0800, false },
        { 0xE000, 0x0800, false },
        { 0xE800, 0x0800, false },
        { 0xF000, 0x0800, false },
        { 0xF800, 0x0400, false },
};
#define UMB_BLOCKS_COUNT (sizeof(umb_blocks) / sizeof(umb_t))

static int umb_blocks_allocated = 0;


umb_t *get_free_umb_block(uint16_t size) {
    for (int i = umb_blocks_allocated; i < UMB_BLOCKS_COUNT; i++)
        if (umb_blocks[i].allocated == false && umb_blocks[i].size <= size) {
            return &umb_blocks[i];
        }
    return NULL;
}


#define XMS_VERSION 0x00
#define REQUEST_HMA 0x01
#define RELEASE_HMA 0x02
#define ENABLE_A20 0x05
#define DISABLE_A20 0x06
#define QUERY_A20 0x07
#define ALLOCATE_EMB 0x09
#define REQUEST_UMB 0x10
#define RELEASE_UMB 0x11

uint8_t xms_handler() {
    switch (CPU_AH) {
        case XMS_VERSION: { // Get XMS Version
            CPU_AX = 0x0100;
            CPU_BX = 0x0001; // driver version
            CPU_DX = 0x0001; // HMA Exist
            break;
        }
        case REQUEST_HMA: { // Request HMA
            // Stub: Implement HMA request functionality
            CPU_AX = 1; // Success
            break;
        }
        case RELEASE_HMA: { // Release HMA
            // Stub: Implement HMA release functionality
            CPU_AX = 1; // Success
            break;
        }
        case ENABLE_A20: { // Local Enable A20
            CPU_AX = 0x0001; // Success
            CPU_BL = 0x00;
            break;
        }
        case DISABLE_A20: { // Local Disable A20
            CPU_AX = 1; // Failed
            CPU_BL = 0;
            break;
        }
        case QUERY_A20: { // Query A20 (Function 07h):
            CPU_AX = 1; // Success
            break;
        }
        case ALLOCATE_EMB: { // Allocate Extended Memory Block (Function 09h):
            // Stub: Implement allocating extended memory block functionality
            CPU_DX = 0x0000;
            CPU_AX = 0x0000;
            CPU_BL = 0xA0;
            break;
        }
        case REQUEST_UMB: { // Request Upper Memory Block (Function 10h):
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

        case RELEASE_UMB: { // Release Upper Memory Block (Function 11h)
            // Stub: Release Upper Memory Block
            for (int i = 0; i < UMB_BLOCKS_COUNT; i++)
                if (umb_blocks[i].segment == CPU_BX && umb_blocks[i].allocated) {
                    umb_blocks[i].allocated = false;
                    umb_blocks_allocated--;
                    CPU_AX = 0x0001; // Success
                    CPU_BL = 0;
                    break;
                }

            CPU_AX = 0x0000; // Failure
            CPU_DX = 0x0000;
            CPU_BL = 0xB2; // Error code
            break;
        }
        default: {
            // Unhandled function
            CPU_AX = 0x0001; // Function not supported
            CPU_BL = 0x80; // Function not implemented
            break;
        }
    }
    return 0xCB; // RETF opcode
}