#pragma once
#pragma GCC optimize("Ofast")
// #define USE_SAMPLES
#if defined(USE_SAMPLES)
#include "emulator/drum/drum.h"
#include "emulator/acoustic/acoustic.h"
#endif
#define MAX_MIDI_VOICES 24
#define MIDI_CHANNELS 16

struct midi_voice_s {
    uint8_t playing;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    int32_t frequency_m100;
    int32_t sample_position;
} midi_voices[MAX_MIDI_VOICES] = {0};

typedef struct midi_channel_s {
    uint8_t program;
    uint8_t sustain;
    uint8_t volume;
    int pitch;
} midi_channel_t;

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint8_t note;
    uint8_t velocity;
    uint8_t other;
} midi_command_t;

midi_channel_t midi_channels[MIDI_CHANNELS] = {0};

// Bitmask for active voices
uint32_t active_voice_bitmask = 0;
uint32_t channels_sustain_bitmask = 0;

static const int32_t note_frequencies_m_100[128] = {
    818, 866, 918, 972, 1030, 1091, 1156, 1225, 1298, 1375, 1457, 1543, 1635, 1732, 1835, 1945, 2060, 2183, 2312, 2450,
    2596, 2750, 2914, 3087, 3270, 3465, 3671, 3889, 4120, 4365, 4625, 4900, 5191, 5500, 5827, 6174, 6541, 6930, 7342,
    7778, 8241, 8731, 9250, 9800, 10383, 11000, 11654, 12347, 13081, 13859, 14683, 15556, 16481, 17461, 18500, 19600,
    20765, 22000, 23308, 24694, 26163, 27718, 29366, 31113, 32963, 34923, 36999, 39200, 41530, 44000, 46616, 49388,
    52325, 55437, 58733, 62225, 65926, 69846, 73999, 78399, 83061, 88000, 93233, 98777, 104650, 110873, 117466, 124451,
    131851, 139691, 147998, 156798, 166122, 176000, 186466, 197553, 209300, 221746, 234932, 248902, 263702, 279383,
    295996, 313596, 332244, 352000, 372931, 395107, 418601, 443492, 469864, 497803, 527404, 558765, 591991, 627193,
    664488, 704000, 745862, 790213, 837202, 886984, 939727, 995606, 1054808, 1117530, 1183982, 1254385
};
static const int8_t sin_m128[1024] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09,
    0x09, 0x09, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12,
    0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x13, 0x14, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x16, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17, 0x17, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x19, 0x19, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x23, 0x23, 0x23, 0x23, 0x23, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x25, 0x25, 0x25, 0x25, 0x25, 0x26, 0x26, 0x26, 0x26, 0x26, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
    0x28, 0x28, 0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x29, 0x29, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
    0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39,
    0x39, 0x39, 0x39, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3C,
    0x3C, 0x3C, 0x3C, 0x3C, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46,
    0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x49, 0x49, 0x49, 0x49,
    0x49, 0x49, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4B, 0x4B, 0x4B, 0x4B, 0x4B, 0x4B, 0x4C,
    0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4E, 0x4E, 0x4E, 0x4E,
    0x4E, 0x4E, 0x4F, 0x4F, 0x4F, 0x4F, 0x4F, 0x4F, 0x4F, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x51,
    0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x53, 0x53, 0x53, 0x53,
    0x53, 0x53, 0x53, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x58,
    0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5B, 0x5B, 0x5B, 0x5B, 0x5B, 0x5B, 0x5B, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5D, 0x5D, 0x5D, 0x5D, 0x5D, 0x5D, 0x5D, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E,
    0x5E, 0x5E, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
    0x60, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62, 0x62,
    0x62, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
    0x64, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x68, 0x68, 0x68, 0x68, 0x68,
    0x68, 0x68, 0x68, 0x68, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x6A, 0x6A, 0x6A,
    0x6A, 0x6A, 0x6A, 0x6A, 0x6A, 0x6A, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6C,
    0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D,
    0x6D, 0x6D, 0x6D, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6F, 0x6F, 0x6F,
    0x6F, 0x6F, 0x6F, 0x6F, 0x6F, 0x6F, 0x6F, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
    0x70, 0x70, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72,
    0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74,
    0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76,
    0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77, 0x77, 0x77,
    0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
    0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79,
    0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A,
    0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B,
    0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C,
    0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C, 0x7C,
    0x7C, 0x7C, 0x7C, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D,
    0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F
};


#define SET_ACTIVE_VOICE(idx) (active_voice_bitmask |= (1U << (idx)))
#define CLEAR_ACTIVE_VOICE(idx) (active_voice_bitmask &= ~(1U << (idx)))
#define IS_ACTIVE_VOICE(idx) ((active_voice_bitmask & (1U << (idx))) != 0)

#define SET_CHANNEL_SUSTAIN(idx) (channels_sustain_bitmask |= (1U << (idx)))
#define CLEAR_CHANNEL_SUSTAIN(idx) (channels_sustain_bitmask &= ~(1U << (idx)))
#define IS_CHANNEL_SUSTAIN(idx) ((channels_sustain_bitmask & (1U << (idx))) != 0)


static INLINE int8_t sin_m_128(size_t idx) {
    if (idx < 1024) return sin_m128[idx];
    if (idx < 2048) return sin_m128[2047 - idx];
    if (idx < (2048 + 1024)) return -sin_m128[idx - 2048];
    return -sin_m128[4095 - idx];
}

static INLINE int32_t sin100sf_m_128_t(int32_t a) {
    static const size_t m0 = SOUND_FREQUENCY * 100 / 4096;
    return sin_m_128((a / m0) & 4095);
}

int16_t midi_sample() {
    int32_t sample = 0;
    struct midi_voice_s *voice = (struct midi_voice_s *) midi_voices;
    for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
        if (IS_ACTIVE_VOICE(voice_number) || IS_CHANNEL_SUSTAIN(voice->channel)) {
#if defined(USE_SAMPLES)
            if (channel->program < 7) {
                const int16_t* smpl = (int16_t*)_Yamaha_TG77_Ivory_Piano_C6_wav; /// _Casio_VZ_10M_Piano_C2_wav;
                size_t z = __fast_mul(voice->frequency_m100, voice->sample_position++) / 104650;
                if (z < 217022 / 2)
                    sample += __fast_mul(voice->velocity, smpl[z]);  /// C6 == 1046.5 Hz;  6541]; // C2 == 65.41Hz
                else
                    voice->playing = 0;
            } else if (voice->channel == 9) {
                const int8_t* smpl;
                switch (voice->note) {
                    case 35: smpl = _35_wav; break;
                    case 36: smpl = _36_wav; break;
                    case 37: smpl = _37_wav; break;
                    case 38: smpl = _38_wav; break;
                    case 39: smpl = _39_wav; break;
                    case 40: smpl = _40_wav; break;
                    case 41: smpl = _41_wav; break;
                    case 42: smpl = _42_wav; break;
                    case 43: smpl = _43_wav; break;
                    case 44: smpl = _44_wav; break;
                    case 45: smpl = _45_wav; break;
                    case 46: smpl = _46_wav; break;
                    case 47: smpl = _47_wav; break;
                    case 48: smpl = _48_wav; break;
                    case 49: smpl = _49_wav; break;
                    //case 50: smpl = _50_wav; break;
                    case 51: smpl = _51_wav; break;
                    case 52: smpl = _52_wav; break;
                    case 53: smpl = _53_wav; break;
                    case 54: smpl = _54_wav; break;
                    case 55: smpl = _55_wav; break;
                    case 56: smpl = _56_wav; break;
                    case 57: smpl = _57_wav; break;
                    case 58: smpl = _58_wav; break;
                    case 59: smpl = _59_wav; break;
                    case 60: smpl = _60_wav; break;
                    case 61: smpl = _61_wav; break;
                    case 62: smpl = _62_wav; break;
                    case 63: smpl = _63_wav; break;
                    case 64: smpl = _64_wav; break;
                    case 65: smpl = _65_wav; break;
                    case 66: smpl = _66_wav; break;
                    case 67: smpl = _67_wav; break;
                    case 68: smpl = _68_wav; break;
                    case 69: smpl = _69_wav; break;
                    case 70: smpl = _70_wav; break;
                    case 71: smpl = _71_wav; break;
                    case 72: smpl = _72_wav; break;
                    case 73: smpl = _73_wav; break;
                    case 74: smpl = _74_wav; break;
                    case 75: smpl = _75_wav; break;
                    case 76: smpl = _76_wav; break;
                    case 77: smpl = _77_wav; break;
                    case 78: smpl = _78_wav; break;
                    case 79: smpl = _79_wav; break;
                    case 80: smpl = _80_wav; break;
                    case 81: smpl = _81_wav; break;
                    default: smpl = _ZZ_Default_wav; break;
                }
                sample += __fast_mul(voice->velocity, smpl[voice->sample_position++]) << 8; // TODO: sz
            } else
#endif
            {
                sample += __fast_mul(voice->velocity, sin100sf_m_128_t(__fast_mul(voice->frequency_m100, voice->sample_position++))) << 8;
            }
        }
        voice++;
    }
    return sample >> 11; // / 128 * 32 + >> 8
}

static INLINE int32_t apply_pitch_bend(const int32_t original_freq_m_100, int deviation_percent) {
    return original_freq_m_100;
    // Select multiplier based on deviation direction using a conditional expression
    const int bend_factor = deviation_percent > 0 ? 123 : 109;

    // Calculate pitch bend factor and scale by 1000
    const int32_t pitch_bend_factor = 1000 + __fast_mul(deviation_percent, bend_factor) / 100;

    // Return frequency adjusted with pitch bend
    return __fast_mul(original_freq_m_100, pitch_bend_factor) / 1000;
}

static INLINE void parse_midi(const uint32_t midi_command) {
    const midi_command_t *message = (midi_command_t *) &midi_command;
    const uint8_t channel = message->command & 0xf;
    // struct midi_voice_s *channel = &midi_voices[message->command & 0xf];
    switch (message->command >> 4) {
        case 0x8: // Note OFF
            for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
                struct midi_voice_s *voice = &midi_voices[voice_number];
                if (IS_ACTIVE_VOICE(voice_number) && voice->note == message->note) {
                    voice->playing = 0;
                    CLEAR_ACTIVE_VOICE(voice_number);
                    break;
                }
            }
            break;
        case 0x9: // Note ON
            for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
                struct midi_voice_s *voice = &midi_voices[voice_number];
                if (!IS_ACTIVE_VOICE(voice_number)) {
                    voice->playing = 1;
                    voice->channel = channel;
                    voice->sample_position = 0;
                    voice->note = message->note;

                    voice->frequency_m100 = apply_pitch_bend(
                        note_frequencies_m_100[message->note], midi_channels[channel].pitch
                    );

                    if (midi_channels[voice->channel].volume) {
                        voice->velocity = midi_channels[channel].volume * message->velocity >> 7;
                    } else {
                        voice->velocity = message->velocity;
                    }
                    SET_ACTIVE_VOICE(voice_number);
                    break;
                }
            }

            break;

        case 0xC: {
            midi_channels[channel].program = message->note;
            // printf("channel %i program %i\n", message->command & 0xf, message->note);
            break;
        }
        // MIDI Controller message
        case 0xB: {
            switch (message->note) {
                case 0x0A:
                    //  Left-rigt pan
                    break;
                case 0x7:
                    // printf("channel %i volume %i\n", channel, midi_channels[channel].volume);
                    midi_channels[channel].volume = message->velocity;
                    for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
                        if (midi_voices[voice_number].channel == channel) {
                            midi_voices[voice_number].velocity =
                                    message->velocity * midi_voices[voice_number].velocity >> 7;
                        }
                    }
                    break;
                case 0x40:
                    midi_channels[channel].sustain = message->velocity & 64;
                    if (message->velocity & 64) {
                        SET_CHANNEL_SUSTAIN(channel);
                    } else {
                        CLEAR_CHANNEL_SUSTAIN(channel);
                    }
                // printf("channel %i sustain %i\n", channel, midi_channels[channel].sustain);
                    break;
                case 0x78:
                case 0x7b:
                    for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number)
                        // if (midi_voices[voice_number].channel == channel)
                        midi_voices[voice_number].playing = 0;

                    active_voice_bitmask = 0;
                    break;
                case 0x79: // all controllers off
                    memset(midi_channels, 0, sizeof(midi_channels));
                    break;
                default:
                    // printf("unknown controller %x\n", message->note);

            }
            break;
        }
        case 0xE:
            // should it take base freq or current?
            midi_channels[channel].pitch = __fast_mul(message->note * 128 + message->velocity - 8192, 100) / 8192;
        // printf("channel %i pitch %i\n", message->command & 0xf, midi_channels[message->command & 0xf].pitch);

            for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number)
                if (midi_voices[voice_number].channel == channel) {
                    // printf("123 channel %i pitch %i\n", message->command & 0xf, midi_channels[message->command & 0xf].pitch);
                    midi_voices[voice_number].frequency_m100 = apply_pitch_bend(
                        midi_voices[voice_number].frequency_m100, midi_channels[channel].pitch);
                }
            break;

        default:
#ifdef DEBUG_MPU401
            printf("Unknown command %x message %04x \n", message->command >> 4, midi_command);
#endif
            break;
    }
    // printf("%x %x %x %x %x\n", message->command, message->note, message->velocity, message->other, midi_command );
}
