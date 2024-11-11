/****************************************************************************

  emu76489.c -- SN76489 emulator by Mitsutaka Okazaki 2001-2016

  2001 08-13 : Version 1.00
  2001 10-03 : Version 1.01 -- Added SNG_set_quality().
  2004 05-23 : Version 1.10 -- Implemented GG stereo mode by RuRuRu
  2004 06-07 : Version 1.20 -- Improved the noise emulation.
  2015 12-13 : Version 1.21 -- Changed own integer types to C99 stdint.h types.
  2016 09-06 : Version 1.22 -- Support per-channel output.

  References: 
    SN76489 data sheet   
    sn76489.c   -- from MAME
    sn76489.txt -- from http://www.smspower.org/

*****************************************************************************/
// https://github.com/mamedev/mame/blob/master/src/devices/sound/sn76496.cpp
// https://www.zeridajh.org/articles/me_sn76489_sound_chip_details/index.html
#include "../emulator.h"

/*
 *The SN76489 is connected to a clock signal, which is commonly 3579545Hz for NTSC systems and 3546893Hz for PAL/SECAM systems (these are based on the associated TV colour subcarrier frequencies, and are common master clock speeds for many systems). It divides this clock by 16 to get its internal clock. The datasheets specify a maximum of 4MHz.
*/
#define BASE_INCREMENT (uint32_t) ((double) 3579545 * (1 << GETA_BITS) / (16 * SOUND_FREQUENCY));
static const uint8_t parity[10] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0 };
static uint32_t sn_count[3];
static uint32_t volume[3];
static uint32_t sn_[3];
static uint32_t edge[3];
static uint32_t mute[3];

static uint32_t noise_seed;
static uint32_t noise_count;
static uint32_t noise_freq;
static uint32_t noise_volume;
static uint32_t noise_mode;
static  uint32_t noise_fref;

static uint32_t base_count;

/* rate converter */
static uint32_t realstep;
static uint32_t sngtime;
static uint32_t sngstep;

static uint32_t addr;

static uint32_t stereo;

static int16_t channel_sample[4];
// } sng = { 0 };


static const uint16_t volume_table[16] = {
        0xff, 0xcb, 0xa1, 0x80, 0x65, 0x50, 0x40, 0x33, 0x28, 0x20, 0x19, 0x14, 0x10, 0x0c, 0x0a, 0x00
};

#define GETA_BITS 24

void sn76489_reset() {
    for (int i = 0; i < 3; i++) {
        sn_count[i] = 0;
        sn_[i] = 0;
        edge[i] = 0;
        volume[i] = 0x0f;
        mute[i] = 0;
    }

    addr = 0;

    noise_seed = 0x8000;
    noise_count = 0;
    noise_freq = 0;
    noise_volume = 0x0f;
    noise_mode = 0;
    noise_fref = 0;

    stereo = 0xFF;

    channel_sample[0] = channel_sample[1] = channel_sample[2] = channel_sample[3] = 0;
}

static INLINE void sn76489_out(const uint16_t value) {
    if (value & 0x80) {
        //printf("OK");
        addr = (value & 0x70) >> 4;
        switch (addr) {
            case 0: // tone 0: frequency
            case 2: // tone 1: frequency
            case 4: // tone 2: frequency
                sn_[addr >> 1] = (sn_[addr >> 1] & 0x3F0) | (value & 0x0F);
                break;

            case 1: // tone 0: volume
            case 3: // tone 1: volume
            case 5: // tone 2: volume
                volume[(addr - 1) >> 1] = value & 0xF;
                break;

            case 6: // noise: frequency, mode
                noise_mode = (value & 4) >> 2;

                if ((value & 0x03) == 0x03) {
                    noise_freq = sn_[2];
                    noise_fref = 1;
                } else {
                    noise_freq = 32 << (value & 0x03);
                    noise_fref = 0;
                }

                if (noise_freq == 0)
                    noise_freq = 1;

                noise_seed = 0x8000;
                break;

            case 7: // noise: volume
                noise_volume = value & 0x0f;
                break;
        }
    } else {
        sn_[addr >> 1] = ((value & 0x3F) << 4) | (sn_[addr >> 1] & 0x0F);
    }
}

static INLINE int16_t sn76489_sample() {
    base_count += BASE_INCREMENT;
    const uint32_t incr = (base_count >> GETA_BITS);
    base_count &= (1 << GETA_BITS) - 1;

    /* Noise */
    noise_count += incr;
    if (noise_count & 0x400) {
        if (noise_mode) /* White */
            noise_seed = (noise_seed >> 1) | (parity[noise_seed & 0x0009] << 15);
        else /* Periodic */
            noise_seed = (noise_seed >> 1) | ((noise_seed & 1) << 15);

        if (noise_fref)
            noise_count -= sn_[2];
        else
            noise_count -= noise_freq;
    }

    if (noise_seed & 1) {
        channel_sample[3] += volume_table[noise_volume] << 4;
    }

    channel_sample[3] >>= 1;


    /* Tone */
    for (int i = 0; i < 3; i++) {
        sn_count[i] += incr;
        if (sn_count[i] & 0x400) {
            if (sn_[i] > 1) {
                edge[i] = !edge[i];
                sn_count[i] -= sn_[i];
            } else {
                edge[i] = 1;
            }
        }

        if (edge[i] && !mute[i]) {
            channel_sample[i] += volume_table[volume[i]] << 4;
        }

        channel_sample[i] >>= 1;
    }
    return (int16_t) (channel_sample[0] + channel_sample[1] + channel_sample[2] + channel_sample[3]);
}
