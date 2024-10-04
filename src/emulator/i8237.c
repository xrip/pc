#include "emulator.h"

/*
  XTulator: A portable, open-source 8086 PC emulator.
  Copyright (C)2020 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
	Intel 8237 DMA controller
*/
//#define DEBUG_DMA
static struct dma_channel_s {
    uint32_t page;
    uint32_t address;
    uint32_t reloadaddr;
    int8_t direction;
    uint16_t count;
    uint16_t reloadcount;
    uint8_t autoinit;
    uint8_t mode;
    uint8_t enable;
    uint8_t masked;
    uint8_t dreq;
    uint8_t finished;
    uint8_t transfer_type;
} dma_channels[4];

static uint8_t flipflop, memtomem;

void i8237_reset() {
    memset(dma_channels, 0x00, sizeof(dma_channels));

    dma_channels[0].masked = 1;
    dma_channels[1].masked = 1;
    dma_channels[2].masked = 1;
    dma_channels[3].masked = 1;
}

void i8237_writeport(uint16_t portnum, uint8_t value) {
    uint8_t channel;
#ifdef DEBUG_DMA
    printf("[DMA] Write port 0x%X: %X\n", portnum, value);
#endif
    portnum &= 0x0F;
    switch (portnum) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7: {
            static uint16_t count = 0;
            static uint32_t address = 0;

            channel = (portnum >> 1) & 3;
            if (portnum & 0x01) { //write terminal count
                if (flipflop) {
                    dma_channels[channel].count = (count & 0x00FF) | ((uint16_t) value << 8);
                    dma_channels[channel].reloadcount = dma_channels[channel].count;
                } else {
                    count = (dma_channels[channel].count & 0xFF00) | (uint16_t) value;
                }

            } else {
                if (flipflop) {
                    dma_channels[channel].address = (address & 0x00FF) | ((uint16_t) value << 8);
                    dma_channels[channel].reloadaddr = dma_channels[channel].address;
                } else {
                    address = (dma_channels[channel].address & 0xFF00) | (uint16_t) value;
                }
            }
            #ifdef DEBUG_DMA
            printf("[DMA] Channel %u addr set to %08X %d\r\n", channel, dma_channels[channel].address,
                   dma_channels[channel].count);
            #endif

            dma_channels[channel].finished = 0;
            flipflop ^= 1;
            break;
        }
        case 0x08: //DMA channel 0-3 command register
            memtomem = value & 1;
            break;
        case 0x09: //DMA request register
            dma_channels[value & 3].dreq = (value >> 2) & 1;
            break;
        case 0x0A: //DMA channel 0-3 mask register
            dma_channels[value & 3].masked = (value >> 2) & 1;
            break;
        case 0x0B: //DMA channel 0-3 mode register
            dma_channels[value & 3].transfer_type = (value >> 2) & 3;
            dma_channels[value & 3].autoinit = (value >> 4) & 1;
            dma_channels[value & 3].direction = (value & 0x20) ? -1 : 1; // bit 5
            dma_channels[value & 3].mode = (value >> 6) & 3;
            break;
        case 0x0E: // Mask Reset
            i8237_reset();
            break;
        case 0x0D: // DMA master clear
            i8237_reset();
        case 0x0C: //clear byte pointer flipflop
            flipflop = 0;
            break;
        case 0x0F: //DMA write mask register
            dma_channels[0].masked = value & 1;
            dma_channels[1].masked = (value >> 1) & 1;
            dma_channels[2].masked = (value >> 2) & 1;
            dma_channels[3].masked = (value >> 3) & 1;
            break;
    }
}

void i8237_writepage(uint16_t portnum, uint8_t value) {
    uint8_t channel;
#ifdef DEBUG_DMA
    printf("[DMA] Write port 0x%X: %X\n", portnum, value);
#endif
    value &= 0x0F;
    portnum &= 0x0F;
    switch (portnum) {
        case 0x07:
            channel = 0;
            break;
        case 0x03:
            channel = 1;
            break;
        case 0x01:
            channel = 2;
            break;
        case 0x02:
            channel = 3;
            break;
        default:
            return;
    }
    dma_channels[channel].page = (uint32_t) value << 16;
#ifdef DEBUG_DMA
    printf("[DMA] Channel %u page set to %08X\r\n", channel, dma_channels[channel].page);
#endif
}

uint8_t i8237_readport(uint16_t portnum) {
    uint8_t channel, result = 0xFF;
#ifdef DEBUG_DMA
    printf("[DMA] Read port 0x%X\n", portnum);
#endif

    portnum &= 0x0F;
    switch (portnum) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            channel = (portnum >> 1) & 3;

            if (portnum & 1) { //count
                if (flipflop) {
                    result = (uint8_t) (dma_channels[channel].count >> 8); //TODO: or give back the reload??
                } else {
                    result = (uint8_t) dma_channels[channel].count; //TODO: or give back the reload??
                }
            } else { //address
                if (flipflop) {
                    result = (uint8_t) (dma_channels[channel].address >> 8);
                } else {
                    result = (uint8_t) dma_channels[channel].address;
                }
            }
            flipflop ^= 1;
            break;
        case 0x08: //status register
            result = 0x0F;
    }
    return result;
}

uint8_t i8237_readpage(uint16_t portnum) {
    uint8_t channel;
#ifdef DEBUG_DMA
    printf("[DMA] Read port 0x%X\n", portnum);
#endif
    portnum &= 0x0F;
    switch (portnum) {
        case 0x07:
            channel = 0;
            break;
        case 0x03:
            channel = 1;
            break;
        case 0x01:
            channel = 2;
            break;
        case 0x02:
            channel = 3;
            break;
        default:
            return 0xFF;
    }
    return (uint8_t) (dma_channels[channel].page >> 16);
}

uint8_t i8237_read(uint8_t channel) {
    uint8_t result = 128;
    //TODO: fix commented out stuff
//    if (dma_channel[channel].enable && !dma_channel[channel].terminal)
//    if (!dma_channels[channel].masked)
    {
//        printf("Read from %06X %x\r\n", dma_channels[channel].page + dma_channels[channel].address, dma_channels[channel].count);
      result = read86(dma_channels[channel].page | dma_channels[channel].address);
//    result = read86((uint16_t)(dma_channel[channel].page + dma_channel[channel].address));
        dma_channels[channel].address += dma_channels[channel].direction;
        dma_channels[channel].count--;

        if (dma_channels[channel].count == 0xFFFF) {
            if (dma_channels[channel].autoinit) {
                dma_channels[channel].count = dma_channels[channel].reloadcount;
                dma_channels[channel].address = dma_channels[channel].reloadaddr;
            } else {
                dma_channels[channel].finished = 1; //TODO: does this also happen in autoinit mode?
            }
        }
    }

    return result;
}

void i8237_write(uint8_t channel, uint8_t value) {
    //TODO: fix commented out stuff
//    if (dma_channel[channel].enable && !dma_channel[channel].terminal)
    {
        write86((dma_channels[channel].page | dma_channels[channel].address), value);

//        printf("Write to %06X\r\n", dma_channel[channel].page + dma_channel[channel].address);
        dma_channels[channel].address += dma_channels[channel].direction;
        dma_channels[channel].count--;
        if (dma_channels[channel].count == 0xFFFF) {
            if (dma_channels[channel].autoinit) {
                dma_channels[channel].count = dma_channels[channel].reloadcount;
                dma_channels[channel].address = dma_channels[channel].reloadaddr;
            } else {
                dma_channels[channel].finished = 1; //TODO: does this also happen in autoinit mode?
            }
        }
    }
}
/*
void i8237_init( CPU_t* cpu) {
    i8237_reset(i8237);

    ports_cbRegister(0x00, 16, (void*)i8237_readport, NULL, (void*)i8237_writeport, NULL, i8237);
    ports_cbRegister(0x80, 16, (void*)i8237_readpage, NULL, (void*)i8237_writepage, NULL, i8237);
}
 */