#pragma once
#include "general-midi.h"
#if !PICO_ON_DEVICE
#define DEBUG_MIDI
#endif
// #define USE_SAMPLES
#if defined(USE_SAMPLES)
#include "emulator/drum/drum.h"
#include "emulator/acoustic/acoustic.h"
#endif

#define MAX_MIDI_VOICES 24
#define MIDI_CHANNELS 16

struct  midi_voice_s {
    uint8_t playing;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t velocity_base;
#if 1
    uint8_t adsr[6];
    uint8_t current_velocity;
#endif
    int32_t frequency_m100;
    uint16_t sample_position;
};
static struct midi_voice_s __not_in_flash("midi") midi_voices[MAX_MIDI_VOICES] = {0};

// Bitmask for active voices
uint32_t __not_in_flash("midi") active_voice_bitmask = 0;

typedef struct midi_channel_s {
    uint8_t program;
    uint8_t volume;
    int pitch;
} midi_channel_t;

// Bitmask for sustained channels
uint32_t __not_in_flash("midi") channels_sustain_bitmask = 0;

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint8_t note;
    uint8_t velocity;
    uint8_t other;
} midi_command_t;

static midi_channel_t __not_in_flash("midi") midi_channels[MIDI_CHANNELS] = {0};


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

#define SIN_STEP (SOUND_FREQUENCY * 100 / 4096)

static INLINE int32_t sin100sf_m_128_t(int32_t a) {
    return sin_m_128((a / SIN_STEP) & 4095);
}

int16_t __time_critical_func() midi_sample() {
    /*int32_t sample1 = 0;
    static unsigned int i = 0;
    static unsigned int note = 0;
    sample1 += __fast_mul(127, sin100sf_m_128_t(__fast_mul(note_frequencies_m_100[note % 128], i)));
    sample1 += __fast_mul(127, sin100sf_m_128_t(__fast_mul(note_frequencies_m_100[note % 128 + 6], i)));
    sample1 += __fast_mul(127, sin100sf_m_128_t(__fast_mul(note_frequencies_m_100[note % 128 + 12], i)));
    if (i++ >= 22050) {
        note++;
        i = 0;
    }


    return (int32_t)(sample1 >> 2);*/
    if (!active_voice_bitmask) return 0;

    int32_t sample = 0;
    uint32_t active_voices = active_voice_bitmask;
    struct midi_voice_s *voice = midi_voices;

    while (active_voices) {
        if (active_voices & 1) {
#if defined(USE_SAMPLES)
            if (midi_channels[voice->channel].program < 7) {
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

            uint16_t sample_position = voice->sample_position++;


            uint8_t * velocity = &voice->velocity;

#ifndef ADSR
            if (sample_position == SOUND_FREQUENCY / 2) {  // poor man ADSR with S only :)
                *velocity -= *velocity >> 2;
            }
#else
            if (sample_position <= SOUND_FREQUENCY / 2)
                switch (sample_position) {
                    case 0: // 0%
                        *velocity = voice->adsr[0];
                    break;
                    case SOUND_FREQUENCY / 16: // 25% of attack
                        *velocity = voice->adsr[1];
                    break;
                    case SOUND_FREQUENCY / 8: // 50% of attack
                        *velocity = voice->adsr[2];
                    break;
                    case SOUND_FREQUENCY / 6: // 75% of attack
                        *velocity = voice->adsr[3];
                    break;
                    case SOUND_FREQUENCY / 4: // 100% of attack
                        velocity = &voice->velocity;
                    break;
                    // Decay phase
                    case SOUND_FREQUENCY / 3:  // 33% of decay
                        *velocity = voice->adsr[4]; // 87.5% volume
                    break;
                    case SOUND_FREQUENCY / 2: // SUSTAIN
                        *velocity = voice->adsr[5]; // 75% volume
                    break;
                }
#endif
            sample += __fast_mul(*velocity, sin100sf_m_128_t(__fast_mul(voice->frequency_m100, sample_position)));
            // printf("sample1 %i %x\n", sample1, sample1);
            // sample += sample1;

            // sample += (*velocity / 127.0) * sin(6.28 * note_frequencies[voice->note] * (sample_position / SOUND_FREQUENCY));
        }
        voice++;
        active_voices >>= 1;
    }

    return (int32_t)(sample >> 2) ; // todo add pass filter
}

// todo: validate is it correct??
static INLINE int32_t apply_pitch(const int32_t base_frequency, const int cents) {
    return cents ? (base_frequency * cents + 5000) / 10000 : base_frequency;
}

static INLINE void parse_midi(const midi_command_t *message) {
    // todo last free/last used -- instead of for loop lookup
    // const midi_command_t *message = (midi_command_t *) &midi_command;
    const uint8_t channel = message->command & 0xf;

    switch (message->command >> 4) {
        case 0x9: // Note ON
            if (message->velocity) {
                for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
                    struct midi_voice_s *voice = &midi_voices[voice_number];
                    if (!IS_ACTIVE_VOICE(voice_number)) {
                        voice->playing = 1;
                        voice->sample_position = 0;
                        voice->channel = channel;
                        voice->note = message->note;

                        voice->frequency_m100 = apply_pitch(
                            note_frequencies_m_100[message->note], midi_channels[channel].pitch
                        );
                        voice->velocity_base = message->velocity;
                        voice->velocity = midi_channels[voice->channel].volume
                                              ? midi_channels[channel].volume * message->velocity >> 7
                                              : message->velocity;
#if 1
                        if (channel != 9) {
                            // Attack
                            voice->adsr[0] = 0; // 0%
                            voice->adsr[1] = voice->velocity / 4; // 25%
                            voice->adsr[2] = voice->velocity / 2; // 50%
                            voice->adsr[3] = voice->velocity - voice->velocity / 4; // 75%
                            // 100% maximum velocity
                            // Decay
                            voice->adsr[4] = voice->velocity - voice->velocity / 8; // 87.5%
                            // Sustain
                            voice->adsr[5] = voice->velocity - voice->velocity / 4; // 75% Sustain
                        } else {
                            // Attack
                            voice->adsr[0] = voice->velocity; // 75%
                            voice->adsr[1] = voice->velocity; // 87.5%
                            voice->adsr[2] = voice->velocity; // 100%
                            voice->adsr[3] = voice->velocity; // 100%
                            // 100% maximum velocity
                            // Decay
                            voice->adsr[4] = voice->velocity; // 87.5%
                            // Sustain
                            voice->adsr[5] = voice->velocity / 2; // 50%
                        }
#endif
                        SET_ACTIVE_VOICE(voice_number);
                        return;
                    }
                }

                break;
            } // else do note off
        case 0x8: // Note OFF
            /* Probably we should
             * Find the first and last entry in the voices list with matching channel, key and look up the smallest play position
             */
            if (!IS_CHANNEL_SUSTAIN(channel))
                for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
                    struct midi_voice_s *voice = &midi_voices[voice_number];
                    if (IS_ACTIVE_VOICE(voice_number) && voice->channel == channel && voice->note == message->note) {
                        voice->playing = 0;
                        CLEAR_ACTIVE_VOICE(voice_number);
                        return;
                    }
                }
            break;


        case 0xB: // Controller Change
            switch (message->note) {
                case 0x7: // Volume change
#ifdef DEBUG_MIDI
                    printf("[MIDI] Channel %i volume %i\n", channel, midi_channels[channel].volume);
#endif
                    midi_channels[channel].volume = message->velocity;
                    for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number) {
                        if (midi_voices[voice_number].channel == channel) {
                            midi_voices[voice_number].velocity = message->velocity * midi_voices[voice_number].velocity_base >> 7;
#if 0
                            midi_voices[voice_number].current_velocity = midi_voices[voice_number].current_velocity * midi_voices[voice_number].velocity >> 7;

                            if (channel != 9) {
                                // Attack
                                midi_voices[voice_number].adsr[0] = 0; // 0%
                                midi_voices[voice_number].adsr[1] = midi_voices[voice_number].velocity / 4; // 25%
                                midi_voices[voice_number].adsr[2] = midi_voices[voice_number].velocity / 2; // 50%
                                midi_voices[voice_number].adsr[3] = midi_voices[voice_number].velocity - midi_voices[voice_number].velocity / 4; // 75%
                                // 100% maximum velocity
                                // Decay
                                midi_voices[voice_number].adsr[4] = midi_voices[voice_number].velocity - midi_voices[voice_number].velocity / 8; // 87.5%
                                // Sustain
                                midi_voices[voice_number].adsr[5] = midi_voices[voice_number].velocity - midi_voices[voice_number].velocity / 4; // 75% Sustain
                            } else {
                                // Attack
                                midi_voices[voice_number].adsr[0] = midi_voices[voice_number].velocity; // 100%
                                midi_voices[voice_number].adsr[1] = midi_voices[voice_number].velocity; // 100%
                                midi_voices[voice_number].adsr[2] = midi_voices[voice_number].velocity; // 100%
                                midi_voices[voice_number].adsr[3] = midi_voices[voice_number].velocity; // 100%
                                // 100% maximum velocity
                                // Decay
                                midi_voices[voice_number].adsr[4] = midi_voices[voice_number].velocity - midi_voices[voice_number].velocity / 4; // 87.5%
                                // Sustain
                                midi_voices[voice_number].adsr[5] = midi_voices[voice_number].velocity / 2; // 50%
                            }
#endif
                        }
                    }
                    break;
                /*
                case 0x0A: //  Left-right pan
                    break;
                */
                case 0x40: // Sustain
                    if (message->velocity & 64) {
                        SET_CHANNEL_SUSTAIN(channel);
                    } else {
                        CLEAR_CHANNEL_SUSTAIN(channel);

                        for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number)
                            if (midi_voices[voice_number].channel == channel) {
                                midi_voices[voice_number].playing = 0;
                                CLEAR_ACTIVE_VOICE(voice_number);
                            }
                    }
#ifdef DEBUG_MIDI
                    printf("[MIDI] Channel %i sustain %i\n", channel, message->velocity);
#endif
                    break;
                case 0x78: // All Sound Off
                case 0x7b: // All Notes Off
                    active_voice_bitmask = 0;
                    channels_sustain_bitmask = 0;
                    memset(midi_voices, 0, sizeof(midi_voices));
                /*
                                for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number)
                                    midi_voices[voice_number].playing = 0;
                */
                    break;
                case 0x79: // all controllers off
                    memset(midi_channels, 0, sizeof(midi_channels));
                    break;
#ifdef DEBUG_MIDI
                default:
                    printf("[MIDI] Unknown channel %i controller %02x %02x\n", channel, message->note,
                           message->velocity);
#endif
            }
            break;

        case 0xC: // Channel Program
            midi_channels[channel].program = message->note;

            for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number)
                if (midi_voices[voice_number].channel == channel) {
                    midi_voices[voice_number].playing = 0;
                    CLEAR_ACTIVE_VOICE(voice_number);
                }

#ifdef DEBUG_MIDI
            printf("[MIDI] Channel %i program %i\n", message->command & 0xf, message->note);
#endif
            break;
        case 0xE:
            const int pitch_bend = (message->velocity * 128 + message->note);
            const int cents = pitch_pows[pitch_bend];
            midi_channels[channel].pitch = cents;
#ifdef DEBUG_MIDI
            printf("[MIDI] Channel %i pitch_bend %i cents %i 44000->%i\n", channel, pitch_bend - 8192, cents,
                   apply_pitch(44000, cents));
#endif
            for (int voice_number = 0; voice_number < MAX_MIDI_VOICES; ++voice_number)
                if (midi_voices[voice_number].channel == channel) {
                    midi_voices[voice_number].frequency_m100 = apply_pitch(
                        midi_voices[voice_number].frequency_m100, cents);
                }
            break;
#ifdef DEBUG_MIDI
        default:
            printf("[MIDI] Unknown channel %i command %x message %04x \n", channel, message->command >> 4,
                   midi_command);
            break;
#endif
    }
}
