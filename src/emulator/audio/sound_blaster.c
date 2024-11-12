// http://qzx.com/pc-gpe/sbdsp.txt
/*
  XTulator: A portable, open-source 80186 PC emulator.
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
	Emulation of the Sound Blaster 2.0
*/

// #define DEBUG_BLASTER

#include "../emulator.h"
static volatile int16_t sample;
static struct sound_blaster_s {
    uint8_t dspenable;


    uint8_t readlen;
    uint8_t readready;
    uint8_t writebuf;
    uint32_t timer;
    uint32_t dmalen;
    uint8_t dmachan;
    uint8_t irq;
    uint8_t lastcmd;
    uint8_t writehilo;
    uint32_t dmacount;
    uint8_t autoinit;
    uint8_t testreg;
    uint8_t silencedsp;
    uint8_t dorecord;
    uint8_t dma_active;
    uint8_t readbuf[32];
} sb;
//#define DEBUG_BLASTER
uint16_t timeconst = 22;
uint64_t sb_samplerate = 22050;

#define SB_irq 3
#define SB_dmachan 1

const int16_t cmd_E2_table[9] = { 0x01, -0x02, -0x04, 0x08, -0x10, 0x20, 0x40, -0x80, -106 };

static INLINE void blaster_putreadbuf(uint8_t value) {
    if (sb.readlen == 32) return;

    sb.readbuf[sb.readlen++] = value;
}

static INLINE uint8_t blaster_getreadbuf() {
    register uint8_t result = *sb.readbuf;

    if (sb.readlen) {
        memmove(sb.readbuf, sb.readbuf + 1, --sb.readlen);
    }

    return result;
}

void blaster_reset() {
    memset(&sb, 0, sizeof(struct sound_blaster_s));

    blaster_putreadbuf(0xAA);
}

static INLINE void blaster_writecmd(uint8_t value) {
    static int i = 0;
//    printf("SB command %x : %x        %d\r\n", sb.lastcmd, value, i++);
    switch (sb.lastcmd) {
        case 0x10: //direct DAC, 8-bit
            sample = value;
            sample -= 128;
            sample *= 256;
            sb.lastcmd = 0;
            return;
        case 0x14: //DMA DAC, 8-bit
        case 0x24:
        case 0x91:
            if (sb.writehilo == 0) {
                sb.dmalen = value;
                sb.writehilo = 1;
            } else {
                sb.dmalen |= (uint32_t) value << 8;
                sb.dmalen++;
                sb.lastcmd = 0;
                sb.dmacount = 0;
                sb.silencedsp = 0;
                sb.autoinit = 0;
                sb.dorecord = (sb.lastcmd == 0x24) ? 1 : 0;
                sb.dma_active = 1;
#ifdef DEBUG_BLASTER
                printf("[BLASTER] Begin DMA transfer mode with 0x%04X  byte blocks\r\n", sb.dmalen);
#endif
            }
            return;
        case 0x40: //set time constant
            timeconst = (256 - value);
//            sb_samplerate = value;
            sb_samplerate = 1000000 / (256 - value);
            //timing_updateIntervalFreq(timer, samplerate);
            sb.lastcmd = 0;
#ifdef DEBUG_BLASTER
            printf("[BLASTER] Set time constant: %u (Sample rate: %lu Hz)\r\n", value, 1000000 / (256 - value));
#endif
            return;
        case 0x48: //set DMA block size
            if (sb.writehilo == 0) {
                sb.dmalen = value;
                sb.writehilo = 1;
            } else {
                sb.dmalen |= (uint32_t) value << 8;
                sb.dmalen++;
                sb.lastcmd = 0;
            }
#ifdef DEBUG_BLASTER
            printf("[NOTICE] Sound Blaster DSP block transfer size set to %u\n", sb.dmalen);
#endif
            return;
        case 0x80: //silence DAC
            if (sb.writehilo == 0) {
                sb.dmalen = value;
                sb.writehilo = 1;
            } else {
                sb.dmalen |= (uint32_t) value << 8;
                sb.dmalen++;
                sb.lastcmd = 0;
                sb.dmacount = 0;
                sb.silencedsp = 1;
                sb.autoinit = 0;
            }
            return;
        case 0xE0: //DSP identification (returns bitwise NOT of data byte)
            blaster_putreadbuf(~value);
            sb.lastcmd = 0;
            return;
        case 0xE2: //DMA identification write
        {
            int16_t val = 0xAA, i;
            for (i = 0; i < 8; i++) {
                if ((value >> i) & 0x01) {
                    val += cmd_E2_table[i];
                }
            }
            val += cmd_E2_table[8];
            i8237_write(SB_dmachan, (uint8_t) val);
            sb.lastcmd = 0;
            return;
        }
        case 0xE4: //write test register
            sb.testreg = value;
            sb.lastcmd = 0;
            return;
    }

    switch (value) {
        case 0x10: //direct DAC, 8-bit
            break;
        case 0x14: //DMA DAC, 8-bit
        case 0x24:
            sb.writehilo = 0;
            break;
        case 0x1C: //auto-initialize DMA DAC, 8-bit
        case 0x2C:
            sb.dmacount = 0;
            sb.silencedsp = 0;
            sb.autoinit = 1;
            sb.dorecord = (value == 0x2C) ? 1 : 0;
            sb.dma_active = 1;
#ifdef DEBUG_BLASTER
            printf("[BLASTER] Begin auto-init DMA transfer mode with %d byte blocks\r\n", sb.dmacount);
#endif
            break;
        case 0x20: //direct DAC, 8-bit record
            blaster_putreadbuf(128); //Silence, though I might add actual recording support later.
            break;
        case 0x40: //set time constant
            break;
        case 0x91:
        case 0x48: //set DMA block size
            sb.writehilo = 0;
            break;
        case 0x80: //silence DAC
            sb.writehilo = 0;
            break;
        case 0xD0: //halt DMA operation, 8-bit
            sb.dma_active = 0;
            break;
        case 0xD1: //speaker on
            sb.dspenable = 1;
            break;
        case 0xD3: //speaker off
            sb.dspenable = 0;
            break;
        case 0xD4: //continue DMA operation, 8-bit
            sb.dma_active = 1;
            break;
        case 0xDA: //exit auto-initialize DMA operation, 8-bit
            sb.dma_active = 0;
            sb.autoinit = 0;
            break;
        case 0xE0: //DSP identification (returns bitwise NOT of data byte)
            break;
        case 0xE1: //DSP version (SB 2.0 is DSP 2.01)
            sb.readlen = 0;
            blaster_putreadbuf(2);
            blaster_putreadbuf(1);
            break;
        case 0xE2: //DMA identification write
            break;
        case 0xE4: //write test register
            break;
        case 0xE8: //read test register
            sb.readlen = 0;
            blaster_putreadbuf(sb.testreg);
            break;
        case 0xF2: //trigger 8-bit IRQ
            doirq(SB_irq);
            break;
        case 0xF8: //Undocumented
            sb.readlen = 0;
            blaster_putreadbuf(0);
            break;
        default:
            break;
//            printf("[BLASTER] Unrecognized command: 0x%02X\r\n", value);
    }

    sb.lastcmd = value;
}

void blaster_write(uint16_t portnum, uint8_t value) {
#ifdef DEBUG_BLASTER1
    printf("[BLASTER] Write %03X: %02X\r\n", portnum, value);
#endif
    portnum &= 0x0F;

    switch (portnum) {
        case 0x06:
            if (value == 0) {
                blaster_reset();
            }
            break;
        case 0x0C: //DSP write (command/data)
            blaster_writecmd(value);
            break;
    }
}

uint8_t blaster_read(uint16_t portnum) {
    uint8_t result = 0xFF;

#ifdef DEBUG_BLASTER1
    printf("[BLASTER] Read %03X\r\n", portnum);
#endif
    portnum &= 0x0F;

    switch (portnum) {
        case 0x0A:
            return blaster_getreadbuf();
        case 0x0C:
            return 0x00;
        case 0x0E:
            return (sb.readlen > 0) ? 0x80 : 0x00;
    }

    return result;
}
extern uint8_t i8237_active();
int16_t blaster_sample() { //for DMA mode
    if (i8237_active()) return 0;
    if (sb.silencedsp == 0) {
        if (sb.dorecord == 0) {
            sample = i8237_read(SB_dmachan);
            sample -= 128;
            sample <<= 6;
        } else {
            i8237_write(SB_dmachan, 128); //silence
        }
    } else {
        sample = 0;
    }

    if (++sb.dmacount == sb.dmalen) {
        sb.dmacount = 0;
        doirq(SB_irq);
        if (sb.autoinit == 0) {
            sb.dma_active = 0;
        }
    }

    if (sb.dspenable == 0) {
        sample = 0;
    }
    return sample;
}
