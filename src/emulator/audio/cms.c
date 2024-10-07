// http://qzx.com/pc-gpe/gameblst.txt
#include <stdint.h>

#define MASTER_CLOCK 7159090
// "Creative Music System / Game Blaster"
//typedef struct cms_t {
static int addrs[2] = { 0 };
static uint8_t regs[2][32] = { 0 };
static uint16_t latch[2][6] = { 0 };
static int freq[2][6] = { 0 };
static float count[2][6] = { 0 };
static int vol[2][6][2] = { 0 };
static int stat[2][6] = { 0 };
static uint16_t noise[2][2] = { 0 };
static uint16_t noisefreq[2][2] = { 0 };
static int noisecount[2][2] = { 0 };
static int noisetype[2][2] = { 0 };

uint8_t latched_data;

//} cms_t;
int16_t out_l = 0, out_r = 0;

static inline void cms_update() {
    // may be in core #0 and in core #1
    int channel, d;
    out_l = 0;
    out_r = 0;

    for (channel = 0; channel < 4; channel++) {
        switch (noisetype[channel >> 1][channel & 1]) {
            case 0:
                noisefreq[channel >> 1][channel & 1] = (MASTER_CLOCK / 256);
                break;
            case 1:
                noisefreq[channel >> 1][channel & 1] = (MASTER_CLOCK / 512);
                break;
            case 2:
                noisefreq[channel >> 1][channel & 1] = (MASTER_CLOCK / 1024);
                break;
            case 3:
                noisefreq[channel >> 1][channel & 1] = freq[channel >> 1][(channel & 1) ? 3 : 0];
                break;
        }
    }
    for (channel = 0; channel < 2; channel++) {
        if (regs[channel][0x1C] & 1) {
            for (d = 0; d < 6; d++) {
                if (regs[channel][0x14] & (1 << d)) {
                    if (stat[channel][d]) {
                        out_l += (vol[channel][d][0] << 6) + (vol[channel][d][0] << 5);
                        out_r += (vol[channel][d][1] << 6) + (vol[channel][d][1] << 5);
                    }
                    count[channel][d] += freq[channel][d];
                    if (count[channel][d] >= 24000) {
                        count[channel][d] -= 24000;
                        stat[channel][d] ^= 1;
                    }
                } else if (regs[channel][0x15] & (1 << d)) {
                    if (noise[channel][d / 3] & 1) {
                        out_l += ((vol[channel][d][0] << 6) + (vol[channel][d][0] << 5));
                        out_r += ((vol[channel][d][1] << 6) + (vol[channel][d][1] << 5));
                    }
                }
            }
            for (d = 0; d < 2; d++) {
                noisecount[channel][d] += noisefreq[channel][d];
                while (noisecount[channel][d] >= 24000) {
                    noisecount[channel][d] -= 24000;
                    noise[channel][d] <<= 1;
                    if (!(((noise[channel][d] & 0x4000) >> 8) ^ (noise[channel][d] & 0x40)))
                        noise[channel][d] |= 1;
                }
            }
        }
    }
}

void cms_samples(int16_t *output) {
    // core #1
    cms_update();
    *output++ += (out_r << 2);
    *output += (out_l << 2);
}

void cms_out(uint16_t addr, uint16_t value) {
    // core #0
    int voice;
    int chip = (addr & 2) >> 1;

    // printf("cms_write : addr %04X val %02X\n", addr & 0xf, value);

    switch (addr & 0xf) {
        case 1:
            addrs[0] = value & 31;
            break;
        case 3:
            addrs[1] = value & 31;
            break;

        case 0:
        case 2:
//            cms_update();
            regs[chip][addrs[chip] & 31] = value;
            switch (addrs[chip] & 31) {
                case 0x00:
                case 0x01:
                case 0x02: /*Volume*/
                case 0x03:
                case 0x04:
                case 0x05:
                    voice = addrs[chip] & 7;
                    vol[chip][voice][0] = value & 0xf;
                    vol[chip][voice][1] = value >> 4;
                    break;
                case 0x08:
                case 0x09:
                case 0x0A: /*Frequency*/
                case 0x0B:
                case 0x0C:
                case 0x0D:
                    voice = addrs[chip] & 7;
                    latch[chip][voice] = (latch[chip][voice] & 0x700) | value;
                    freq[chip][voice] =
                            (MASTER_CLOCK / 512 << (latch[chip][voice] >> 8)) / (511 - (latch[chip][voice] & 255));
                    break;
                case 0x10:
                case 0x11:
                case 0x12: /*Octave*/
                    voice = (addrs[chip] & 3) << 1;
                    latch[chip][voice] = (latch[chip][voice] & 0xFF) | ((value & 7) << 8);
                    latch[chip][voice + 1] = (latch[chip][voice + 1] & 0xFF) | ((value & 0x70) << 4);
                    freq[chip][voice] =
                            (MASTER_CLOCK / 512 << (latch[chip][voice] >> 8)) / (511 - (latch[chip][voice] & 255));
                    freq[chip][voice + 1] = (MASTER_CLOCK / 512 << (latch[chip][voice + 1] >> 8)) /
                                            (511 - (latch[chip][voice + 1] & 255));
                    break;
                case 0x16: /*Noise*/
                    noisetype[chip][0] = value & 3;
                    noisetype[chip][1] = (value >> 4) & 3;
                    break;
            }
            break;
        case 0x6:
        case 0x7:
            latched_data = value;
            break;
    }
}

uint8_t cms_in(uint16_t addr) {
    // core #0
    //printf("cms_read : addr %04X\n", addr & 0xf);
    switch (addr & 0xf) {
        case 0x1:
            return addrs[0];
        case 0x3:
            return addrs[1];
        case 0x4:
            return 0x7f;
        case 0xa:
        case 0xb:
            return latched_data;
    }
    return 0xff;
}
