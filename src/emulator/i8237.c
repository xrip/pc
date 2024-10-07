// https://wiki.osdev.org/ISA_DMA
// https://pdos.csail.mit.edu/6.828/2004/readings/hardware/8237A.pdf
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
struct dma_channel_s {
    uint32_t page;
    uint16_t address;
    uint16_t reload_address;
    uint32_t address_increase;
    uint16_t count;
    uint16_t reload_count;
    uint8_t autoinit;
    uint8_t mode;
    uint8_t enable;
    uint8_t masked;
    uint8_t dreq;
    uint8_t finished;
    uint8_t transfer_type;
} dma_channels[4] = { 0 };

static uint8_t flipflop, memtomem;

void i8237_reset() {
    memset(dma_channels, 0x00, sizeof(struct dma_channel_s) * 4);

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
                    dma_channels[channel].reload_count = dma_channels[channel].count;
                } else {
                    count = (dma_channels[channel].count & 0xFF00) | (uint16_t) value;
                }

            } else {
                if (flipflop) {
                    dma_channels[channel].address = (address & 0x00FF) | ((uint16_t) value << 8);
                    dma_channels[channel].reload_address = dma_channels[channel].address;
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
            dma_channels[value & 3].address_increase = (value & 0x20) ? 0xFFFFFFFF : 0x1; // bit 5
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

inline static void update_count(uint8_t channel) {
    dma_channels[channel].address += dma_channels[channel].address_increase;
    dma_channels[channel].count--;

    if ((dma_channels[channel].count == 0xFFFF) && (dma_channels[channel].autoinit)) {
        dma_channels[channel].count   = dma_channels[channel].reload_count;
        dma_channels[channel].address = dma_channels[channel].reload_address;
    }
}

uint8_t i8237_read(uint8_t channel) {
//        printf("Read from %06X %x\r\n", dma_channels[channel].page + dma_channels[channel].address, dma_channels[channel].count);
    register uint32_t address = dma_channels[channel].page + dma_channels[channel].address;
    update_count(channel);
    return read86(address & 0xfffff);
}

void i8237_write(uint8_t channel, uint8_t value) {
    //        printf("Write to %06X %x value %x\r\n", dma_channels[channel].page + dma_channels[channel].address, dma_channels[channel].count, value);
    register uint32_t address = dma_channels[channel].page + dma_channels[channel].address;
    update_count(channel);
    write86(address & 0xfffff, value);

}
