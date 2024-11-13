#pragma once
#pragma GCC optimize("Ofast")
// http://qzx.com/pc-gpe/gameblst.txt

#define MASTER_CLOCK 7159090
#define NOISE_FREQ_256 (MASTER_CLOCK / 256)
#define NOISE_FREQ_512 (MASTER_CLOCK / 512)
#define NOISE_FREQ_1024 (MASTER_CLOCK / 1024)

static int addrs[2] = {0};
static uint8_t cms_regs[2][32] = {0};
static uint16_t latch[2][6] = {0};
static int freq[2][6] = {0};
static int count[2][6] = {0};
static int vol[2][6][2] = {0};
static int stat[2][6] = {0};
static uint16_t noise[2][2] = {0};
static uint16_t noisefreq[2][2] = {0};
static int noisecount[2][2] = {0};
static uint8_t noisetype[2][2] = {0};

static uint8_t latched_data;

//} cms_t;
// static int16_t out_l = 0, out_r = 0;
static INLINE void cms_samples(int16_t *output) {
    for (int channel = 0; channel < 4; channel++) {
        const uint8_t chip_idx = channel >> 1;
        const uint8_t noise_idx = channel & 1;

        switch (noisetype[chip_idx][noise_idx]) {
            case 0: noisefreq[chip_idx][noise_idx] = NOISE_FREQ_256;
                break;
            case 1: noisefreq[chip_idx][noise_idx] = NOISE_FREQ_512;
                break;
            case 2: noisefreq[chip_idx][noise_idx] = NOISE_FREQ_1024;
                break;
            case 3: noisefreq[chip_idx][noise_idx] = freq[chip_idx][noise_idx ? 3 : 0];
                break;
        }

        if (channel < 2 && (cms_regs[channel][0x1C] & 1)) {
            for (int d = 0; d < 6; d++) {
                const int vol_left = (vol[channel][d][0] << 6) + (vol[channel][d][0] << 5);
                const int vol_right = (vol[channel][d][1] << 6) + (vol[channel][d][1] << 5);

                if (cms_regs[channel][0x14] & (1 << d)) {
                    if (stat[channel][d]) {
                        output[0] += vol_left;
                        output[1] += vol_right;
                    }
                    count[channel][d] += freq[channel][d];
                    if (count[channel][d] >= 24000) {
                        count[channel][d] -= 24000;
                        stat[channel][d] ^= 1;
                    }
                } else if (cms_regs[channel][0x15] & (1 << d)) {
                    if (noise[channel][d / 3] & 1) {
                        output[0] += vol_left;
                        output[1] += vol_right;
                    }
                }

                if (d < 2) {
                    noisecount[channel][d] += noisefreq[channel][d];
                    while (noisecount[channel][d] >= 24000) {
                        noisecount[channel][d] -= 24000;
                        noise[channel][d] = noise[channel][d] << 1 | !(
                                                (noise[channel][d] & 0x4000) >> 8 ^ noise[channel][d] & 0x40);
                    }
                }
            }
        }
    }
}

static INLINE void cms_out(const uint16_t addr, const uint16_t value) {
    int chip = (addr & 2) >> 1;
    int voice;

    switch (addr & 0xf) {
        case 1: addrs[0] = value & 31;
            break;
        case 3: addrs[1] = value & 31;
            break;

        case 0:
        case 2:
            cms_regs[chip][addrs[chip] & 31] = value;
            switch (addrs[chip] & 31) {
                case 0x00 ... 0x05: // Volume control
                    voice = addrs[chip] & 7;
                    vol[chip][voice][0] = value & 0xf;
                    vol[chip][voice][1] = value >> 4;
                    break;

                case 0x08 ... 0x0D: // Frequency control
                    voice = addrs[chip] & 7;
                    latch[chip][voice] = (latch[chip][voice] & 0x700) | value;
                    freq[chip][voice] = (MASTER_CLOCK / 512 << (latch[chip][voice] >> 8)) / (
                                            511 - (latch[chip][voice] & 255));
                    break;

                case 0x10 ... 0x12: // Octave control
                    voice = (addrs[chip] & 3) << 1;
                    latch[chip][voice] = (latch[chip][voice] & 0xFF) | ((value & 7) << 8);
                    latch[chip][voice + 1] = (latch[chip][voice + 1] & 0xFF) | ((value & 0x70) << 4);
                    freq[chip][voice] = (MASTER_CLOCK / 512 << (latch[chip][voice] >> 8)) / (
                                            511 - (latch[chip][voice] & 255));
                    freq[chip][voice + 1] = (MASTER_CLOCK / 512 << (latch[chip][voice + 1] >> 8)) / (
                                                511 - (latch[chip][voice + 1] & 255));
                    break;

                case 0x16: // Noise type
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

static INLINE uint8_t cms_in(const uint16_t addr) {
    switch (addr & 0xf) {
        case 0x1: return addrs[0];
        case 0x3: return addrs[1];
        case 0x4: return 0x7f;
        case 0xa:
        case 0xb: return latched_data;
    }
    return 0xff;
}
