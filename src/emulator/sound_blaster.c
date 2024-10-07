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

#include "emulator.h"

uint8_t dspenable;
int16_t sample;
uint8_t readbuf[16];
uint8_t readlen;
uint8_t readready;
uint8_t writebuf;
uint8_t timeconst;
uint64_t sb_samplerate = 22050;
uint32_t timer;
uint32_t dmalen;
uint8_t dmachan = 1;
uint8_t irq = 5;
uint8_t lastcmd;
uint8_t writehilo;
uint32_t dmacount;
uint8_t autoinit;
uint8_t testreg;
uint8_t silencedsp;
uint8_t dorecord;
uint8_t dma_active;
//#define DEBUG_BLASTER

const int16_t cmd_E2_table[9] = { 0x01, -0x02, -0x04, 0x08, -0x10, 0x20, 0x40, -0x80, -106 };

static inline void blaster_putreadbuf(uint8_t value) {
    if (readlen == 16) return;

    readbuf[readlen++] = value;
}

static inline uint8_t blaster_getreadbuf() {
    uint8_t result;

    result = readbuf[0];
    if (readlen > 0) {
        readlen--;
    }
    memmove(readbuf, readbuf + 1, 15);

    return result;
}

void blaster_reset() {
    dspenable = 0;
    sample = 0;
    readlen = 0;
    blaster_putreadbuf(0xAA);
}

static inline void blaster_writecmd(uint8_t value) {
    switch (lastcmd) {
        case 0x10: //direct DAC, 8-bit
            sample = value;
            sample -= 128;
            sample *= 256;
            lastcmd = 0;
            return;
        case 0x14: //DMA DAC, 8-bit
        case 0x24:
            if (writehilo == 0) {
                dmalen = value;
                writehilo = 1;
            } else {
                dmalen |= (uint32_t) value << 8;
                dmalen++;
                lastcmd = 0;
                dmacount = 0;
                silencedsp = 0;
                autoinit = 0;
                dorecord = (lastcmd == 0x24) ? 1 : 0;
                dma_active = 1;
#ifdef DEBUG_BLASTER
                printf("[BLASTER] Begin DMA transfer mode with %lu byte blocks\r\n", dmalen);
#endif
            }
            return;
        case 0x40: //set time constant
            timeconst = value;
//            sb_samplerate = value;
            sb_samplerate = 1000000 / (256 - value);
            //timing_updateIntervalFreq(timer, samplerate);
            lastcmd = 0;
#ifdef DEBUG_BLASTER
            printf("[BLASTER] Set time constant: %u (Sample rate: %lu Hz)\r\n", value, 1000000 / (256 - value));
#endif
            return;
        case 0x48: //set DMA block size
            if (writehilo == 0) {
                dmalen = value;
                writehilo = 1;
            } else {
                dmalen |= (uint32_t) value << 8;
                dmalen++;
                lastcmd = 0;
            }
            return;
        case 0x80: //silence DAC
            if (writehilo == 0) {
                dmalen = value;
                writehilo = 1;
            } else {
                dmalen |= (uint32_t) value << 8;
                dmalen++;
                lastcmd = 0;
                dmacount = 0;
                silencedsp = 1;
                autoinit = 0;
            }
            return;
        case 0xE0: //DSP identification (returns bitwise NOT of data byte)
            blaster_putreadbuf(~value);
            lastcmd = 0;
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
            i8237_write(dmachan, (uint8_t) val);
            lastcmd = 0;
            return;
        }
        case 0xE4: //write test register
            testreg = value;
            lastcmd = 0;
            return;
    }

    switch (value) {
        case 0x10: //direct DAC, 8-bit
            break;
        case 0x14: //DMA DAC, 8-bit
        case 0x24:
            writehilo = 0;
            break;
        case 0x1C: //auto-initialize DMA DAC, 8-bit
        case 0x2C:
            dmacount = 0;
            silencedsp = 0;
            autoinit = 1;
            dorecord = (value == 0x2C) ? 1 : 0;
            dma_active = 1;
#ifdef DEBUG_BLASTER
            printf("[BLASTER] Begin auto-init DMA transfer mode with %lu byte blocks\r\n", dmalen);
#endif
            break;
        case 0x20: //direct DAC, 8-bit record
            blaster_putreadbuf(128); //Silence, though I might add actual recording support later.
            break;
        case 0x40: //set time constant
            break;
        case 0x48: //set DMA block size
            writehilo = 0;
            break;
        case 0x80: //silence DAC
            writehilo = 0;
            break;
        case 0xD0: //halt DMA operation, 8-bit
            dma_active = 0;
            break;
        case 0xD1: //speaker on
            dspenable = 1;
            break;
        case 0xD3: //speaker off
            dspenable = 0;
            break;
        case 0xD4: //continue DMA operation, 8-bit
            dma_active = 1;
            break;
        case 0xDA: //exit auto-initialize DMA operation, 8-bit
            dma_active = 0;
            autoinit = 0;
            break;
        case 0xE0: //DSP identification (returns bitwise NOT of data byte)
            break;
        case 0xE1: //DSP version (SB 2.0 is DSP 2.01)
            blaster_putreadbuf(2);
            blaster_putreadbuf(1);
            break;
        case 0xE2: //DMA identification write
            break;
        case 0xE4: //write test register
            break;
        case 0xE8: //read test register
            blaster_putreadbuf(testreg);
            break;
        case 0xF2: //trigger 8-bit IRQ
            doirq(irq);
            break;
        case 0xF8: //Undocumented
            readlen = 0;
            blaster_putreadbuf(0);
            break;
        default:
            break;
//            printf("[BLASTER] Unrecognized command: 0x%02X\r\n", value);
    }

    lastcmd = value;
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
            return (readlen > 0) ? 0x80 : 0x00;
    }

    return result;
}

int16_t blaster_generateSample() { //for DMA mode
    if (!dma_active) return 0;
    if (silencedsp == 0) {
        if (dorecord == 0) {
            sample = i8237_read(dmachan);
            sample -= 128;
            sample *= 256;
        } else {
            i8237_write(dmachan, 128); //silence
        }
    } else {
        sample = 0;
    }

    if (++dmacount == dmalen) {
        dmacount = 0;
        doirq(irq);
        if (autoinit == 0) {
            dma_active = 0;
        }
    }

    if (dspenable == 0) {
        sample = 0;
    }
    return sample;
}

/*
void blaster_init(I8237_t* i8237, I8259_t* i8259, uint16_t base, uint8_t dma, uint8_t irq) {
    debug_log(DEBUG_INFO, "[BLASTER] Initializing Sound Blaster 2.0 at base port 0x%03X, IRQ %u, DMA %u\r\n", base, irq, dma);
    memset(blaster, 0, sizeof(BLASTER_t));
    i8237 = i8237;
    i8259 = i8259;
    dmachan = dma;
    irq = irq;
    ports_cbRegister(base, 16, (void*)blaster_read, NULL, (void*)blaster_write, NULL, blaster);

    //TODO: error handling
    timer = timing_addTimer(blaster_generateSample, blaster, 22050, TIMING_DISABLED);
}*/