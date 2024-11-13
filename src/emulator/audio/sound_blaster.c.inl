// https://the.earth.li/~tfm/oldpage/sb_dsp.html
// http://qzx.com/pc-gpe/sbdsp.txt
// https://github.com/joncampbell123/dosbox-x/wiki/Hardware:Sound-Blaster:DSP-commands
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
#define SB_READ_BUFFER 16

// Sound Blaster DSP I/O port offsets
#define DSP_RESET           0x6
#define DSP_READ            0xA
#define DSP_WRITE           0xC
#define DSP_WRITE_STATUS    0xC
#define DSP_READ_STATUS     0xE

// Sound Blaster DSP commands.
#define DSP_DMA_HS_SINGLE       0x91
#define DSP_DMA_HS_AUTO         0x90
#define DSP_DMA_ADPCM           0x7F    //creative ADPCM 8bit to 3bit
#define DSP_DMA_SINGLE          0x14    //follosed by length
#define DSP_DMA_AUTO            0X1C    //length based on 48h
#define DSP_DMA_BLOCK_SIZE      0x48    //block size for highspeed/dma
//#define DSP_DMA_DAC 0x14
#define DSP_DIRECT_DAC          0x10
#define DSP_DIRECT_ADC          0x20
#define DSP_MIDI_READ_POLL      0x30
#define DSP_MIDI_WRITE_POLL     0x38
#define DSP_SET_TIME_CONSTANT   0x40
#define DSP_DMA_PAUSE           0xD0
#define DSP_DMA_PAUSE_DURATION  0x80    //Used by Tryrian
#define DSP_ENABLE_SPEAKER      0xD1
#define DSP_DISABLE_SPEAKER     0xD3
#define DSP_DMA_RESUME          0xD4
#define DSP_SPEAKER_STATUS      0xD8
#define DSP_IDENTIFICATION      0xE0
#define DSP_VERSION             0xE1
#define DSP_WRITETEST           0xE4
#define DSP_READTEST            0xE8
#define DSP_SINE                0xF0
#define DSP_IRQ                 0xF2
#define DSP_CHECKSUM            0xF4

typedef  struct sound_blaster_s {
    int16_t sample;

    uint8_t speaker_enabled;

    uint8_t buffer_length;
    uint32_t dma_length;
    uint8_t command;
    uint8_t writehilo;
    uint32_t dma_counter;
    uint8_t dma_auto_init;
    uint8_t test_register;
    uint8_t silencedsp;
    uint8_t recording_audio;
    uint8_t dma_enabled;
    uint8_t buffer[SB_READ_BUFFER];
} sound_blaster_s;

static sound_blaster_s blaster = { 0 };

//#define DEBUG_BLASTER
uint16_t timeconst = 22;
uint64_t sb_samplerate = 22050;

#define SB_IRQ 3
#define SB_DMA_CHANNEL 1


static const int16_t cmd_E2_table[9] = { 0x01, -0x02, -0x04, 0x08, -0x10, 0x20, 0x40, -0x80, -106 };

static INLINE void blaster_write_buffer(const uint8_t value) {
    if (blaster.buffer_length < SB_READ_BUFFER) {
        blaster.buffer[blaster.buffer_length++] = value;
    }
}

static INLINE uint8_t blaster_read_buffer() {
    if (blaster.buffer_length == 0) return 0;

    const uint8_t result = blaster.buffer[0];

    for (int i = 0; i < blaster.buffer_length - 1; i++) {
        blaster.buffer[i] = blaster.buffer[i + 1];
    }

    blaster.buffer_length--;

    return result;
}

INLINE void blaster_reset() {
    memset(&blaster, 0, sizeof(sound_blaster_s));
    blaster.sample = 0;
    blaster_write_buffer(0xAA);
}

static INLINE void blaster_command(uint8_t value) {
    //    printf("SB command %x : %x        %d\r\n", sb.lastcmd, value, i++);
    switch (blaster.command) {
        case DSP_DIRECT_DAC: //direct DAC, 8-bit
            blaster.sample = (value - 128) << 8;
            blaster.command = 0;
            return;
        case DSP_DMA_SINGLE: //DMA DAC, 8-bit
        case 0x24:
        case DSP_DMA_HS_SINGLE:
            if (blaster.writehilo == 0) {
                blaster.dma_length = value;
                blaster.writehilo = 1;
            } else {
                blaster.dma_length |= (uint32_t) value << 8;
                blaster.dma_length++;
                blaster.command = 0;
                blaster.dma_counter = 0;
                blaster.silencedsp = 0;
                blaster.dma_auto_init = 0;
                blaster.recording_audio = (blaster.command == 0x24) ? 1 : 0;
                blaster.dma_enabled = 1;
#ifdef DEBUG_BLASTER
                printf("[BLASTER] Begin DMA transfer mode with 0x%04X  byte blocks\r\n", sb.dmalen);
#endif
            }
            return;
        case DSP_SET_TIME_CONSTANT: //set time constant
            timeconst = 256 - value;
            sb_samplerate = 1000000 / timeconst;
            blaster.command = 0;
#ifdef DEBUG_BLASTER
            printf("[BLASTER] Set time constant: %u (Sample rate: %lu Hz)\r\n", value, 1000000 / (256 - value));
#endif
            return;
        case DSP_DMA_BLOCK_SIZE: //set DMA block size
            if (blaster.writehilo == 0) {
                blaster.dma_length = value;
                blaster.writehilo = 1;
            } else {
                blaster.dma_length |= (uint32_t) value << 8;
                blaster.dma_length++;
                blaster.command = 0;
            }
#ifdef DEBUG_BLASTER
            printf("[NOTICE] Sound Blaster DSP block transfer size set to %u\n", sb.dmalen);
#endif
            return;
        case DSP_DMA_PAUSE_DURATION: //silence DAC
            if (blaster.writehilo == 0) {
                blaster.dma_length = value;
                blaster.writehilo = 1;
            } else {
                blaster.dma_length |= (uint32_t) value << 8;
                blaster.dma_length++;
                blaster.command = 0;
                blaster.dma_counter = 0;
                blaster.silencedsp = 1;
                blaster.dma_auto_init = 0;
            }
            return;
        case DSP_IDENTIFICATION: //DSP identification (returns bitwise NOT of data byte)
            blaster_write_buffer(~value);
            blaster.command = 0;
            return;
        case 0xE2: //DMA identification write
        {
            int16_t val = 0xAA;
            for (uint8_t i = 0; i < 8; i++) {
                if (value >> i & 0x01) {
                    val += cmd_E2_table[i];
                }
            }
            val += cmd_E2_table[8];
            i8237_write(SB_DMA_CHANNEL, val);
            blaster.command = 0;
            return;
        }
        case DSP_WRITETEST: //write test register
            blaster.test_register = value;
            blaster.command = 0;
            return;
    }

    switch (value) {
        case 0x10: //direct DAC, 8-bit
            break;
        case 0x14: //DMA DAC, 8-bit
        case 0x24:
            blaster.writehilo = 0;
            break;
        case DSP_DMA_AUTO: //auto-initialize DMA DAC, 8-bit
        case 0x2C:
            blaster.dma_counter = 0;
            blaster.silencedsp = 0;
            blaster.dma_auto_init = 1;
            blaster.recording_audio = (value == 0x2C) ? 1 : 0;
            blaster.dma_enabled = 1;
#ifdef DEBUG_BLASTER
            printf("[BLASTER] Begin auto-init DMA transfer mode with %d byte blocks\r\n", sb.dmacount);
#endif
            break;
        case DSP_DIRECT_ADC: //direct ADC, 8-bit record
            blaster_write_buffer(128); //Silence, though I might add actual recording support later.
            break;
        case 0x40: //set time constant
            break;
        case 0x91:
        case 0x48: //set DMA block size
            blaster.writehilo = 0;
            break;
        case 0x80: //silence DAC
            blaster.writehilo = 0;
            break;
        case DSP_DMA_PAUSE: //halt DMA operation, 8-bit
            blaster.dma_enabled = 0;
            break;
        case DSP_ENABLE_SPEAKER: //speaker on
            blaster.speaker_enabled = 1;
            break;
        case DSP_DISABLE_SPEAKER: //speaker off
            blaster.speaker_enabled = 0;
            break;
        case DSP_DMA_RESUME: //continue DMA operation, 8-bit
            blaster.dma_enabled = 1;
            break;
        case 0xDA: //exit auto-initialize DMA operation, 8-bit
            blaster.dma_enabled = 0;
            blaster.dma_auto_init = 0;
            break;
        case 0xE0: //DSP identification (returns bitwise NOT of data byte)
            break;
        case DSP_VERSION: //DSP version (SB 2.0 is DSP 2.01)
            blaster_write_buffer(2);
            blaster_write_buffer(1);
            break;
        case 0xE2: //DMA identification write
            break;
        case 0xE4: //write test register
            break;
        case DSP_READTEST: //read test register
            blaster.buffer_length = 0;
            blaster_write_buffer(blaster.test_register);
            break;
        case DSP_IRQ: //trigger 8-bit IRQ
            doirq(SB_IRQ);
            break;
        case 0xF8: //Undocumented
            blaster.buffer_length = 0;
            blaster_write_buffer(0);
            break;
        default:
            break;
//            printf("[BLASTER] Unrecognized command: 0x%02X\r\n", value);
    }

    blaster.command = value;
}

static INLINE void blaster_write(const uint16_t portnum, const uint8_t value) {
#ifdef DEBUG_BLASTER
    printf("[BLASTER] Write %03X: %02X\r\n", portnum, value);
#endif
    switch (portnum & 0xF) {
        case DSP_RESET:
            blaster_reset(value);
            break;
        case DSP_WRITE: //DSP write (command/data)
            blaster_command(value);
            break;
    }
}

static INLINE uint8_t blaster_read(const uint16_t portnum) {
#ifdef DEBUG_BLASTER
    printf("[BLASTER] Read %03X\r\n", portnum);
#endif

    switch (portnum & 0xF) {
        case DSP_READ:
            return blaster_read_buffer();
        case DSP_WRITE_STATUS:
            return 0x00;
        case DSP_READ_STATUS:
            return blaster.buffer_length ? 0x80 : 0x00;
    }

    return 0xff;
}

inline int16_t blaster_sample() { //for DMA mode
    int16_t sample = 0;
    if (!blaster.dma_enabled) return blaster.speaker_enabled ? blaster.sample : 0;
    if (blaster.silencedsp == 0) {
        if (blaster.recording_audio == 0) {
            sample = (i8237_read(SB_DMA_CHANNEL) - 128) << 6;
        } else {
            i8237_write(SB_DMA_CHANNEL, 128); //silence
        }
    }

    if (++blaster.dma_counter == blaster.dma_length) {
        blaster.dma_counter = 0;
        doirq(SB_IRQ);
        blaster.dma_enabled = blaster.dma_auto_init;
    }

    return blaster.speaker_enabled ? sample : 0;
}
