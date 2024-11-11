#pragma GCC optimize("Ofast")
#pragma once
#include "general-midi.h"
#if !PICO_ON_DEVICE

// #define DEBUG_MIDI

#ifdef DEBUG_MIDI
#define debug_log(...) printf(__VA_ARGS__)
#else
#define debug_log(...)
#endif

#endif
// #define USE_SAMPLES
#if defined(USE_SAMPLES)
#include "emulator/drum/drum.h"
#include "emulator/acoustic/acoustic.h"
#endif

#define MAX_MIDI_VOICES 32
#define MIDI_CHANNELS 16

#define RELEASE_DURATION (SOUND_FREQUENCY / 8) // Duration for note release

typedef struct midi_voice_s {
    uint8_t voice_slot;
    // uint8_t playing;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t velocity_base;

    int32_t frequency_m100;
    uint16_t sample_position;
    uint16_t release;
} midi_voice_t;

typedef struct midi_channel_s {
    uint8_t program;
    uint8_t volume;
    int pitch;
} midi_channel_t;

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint8_t note;
    uint8_t velocity;
    uint8_t other;
} midi_command_t;

static midi_voice_t midi_voices[MAX_MIDI_VOICES] = {0};
static midi_channel_t midi_channels[MIDI_CHANNELS] = {0};

// Bitmask for active voices
static uint32_t active_voice_bitmask = 0;
// Bitmask for sustained channels
static uint32_t channels_sustain_bitmask = 0;


#define SET_ACTIVE_VOICE(idx) (active_voice_bitmask |= (1U << (idx)))
#define CLEAR_ACTIVE_VOICE(idx) (active_voice_bitmask &= ~(1U << (idx)))
#define IS_ACTIVE_VOICE(idx) ((active_voice_bitmask & (1U << (idx))) != 0)

#define SET_CHANNEL_SUSTAIN(idx) (channels_sustain_bitmask |= (1U << (idx)))
#define CLEAR_CHANNEL_SUSTAIN(idx) (channels_sustain_bitmask &= ~(1U << (idx)))
#define IS_CHANNEL_SUSTAIN(idx) ((channels_sustain_bitmask & (1U << (idx))) != 0)

static INLINE int8_t sin_m_128(size_t idx) {
    size_t mod_idx = idx & 4095;
    return (mod_idx < 2048)
               ? sin_m128[mod_idx < 1024 ? mod_idx : 2047 - mod_idx]
               : -sin_m128[mod_idx < 3072 ? mod_idx - 2048 : 4095 - mod_idx];
}


#define SIN_STEP (SOUND_FREQUENCY * 100 / 4096)

static INLINE int32_t sin100sf_m_128_t(int32_t a) {
    return sin_m_128((a / SIN_STEP) & 4095);
}

static INLINE int16_t __time_critical_func() midi_sample() {
    if (!active_voice_bitmask) return 0;

    volatile int32_t sample = 0;
    volatile uint32_t active_voices = active_voice_bitmask;

    for (struct midi_voice_s *voice = midi_voices; active_voices; voice++, active_voices >>= 1) {
        if (!(active_voices & 1)) continue;

        const uint16_t sample_position = voice->sample_position++;
        uint8_t *velocity = &voice->velocity;

        if (sample_position == SOUND_FREQUENCY / 2) { // Sustain state
            *velocity -= *velocity >> 2;
        } else if (sample_position && sample_position == voice->release) {
            CLEAR_ACTIVE_VOICE(voice->voice_slot);
        }

        sample += __fast_mul(*velocity, sin100sf_m_128_t(__fast_mul(voice->frequency_m100, sample_position)));
        // sample += (*velocity / 127.0) * sin(2 * PI * note_frequencies[voice->note] * (sample_position / SOUND_FREQUENCY));
    }

    return (int32_t) (sample >> 2); // todo add pass filter
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
                for (int voice_slot = 0; voice_slot < MAX_MIDI_VOICES; ++voice_slot) {
                    if (!IS_ACTIVE_VOICE(voice_slot)) {
                        struct midi_voice_s *voice = &midi_voices[voice_slot];
                        // voice->playing = 1;
                        voice->voice_slot = voice_slot;
                        voice->sample_position = 0;
                        voice->release = 0;
                        voice->channel = channel;
                        voice->note = message->note;

                        voice->frequency_m100 = apply_pitch(
                            note_frequencies_m_100[message->note], midi_channels[channel].pitch
                        );
                        voice->velocity_base = message->velocity;
                        voice->velocity = midi_channels[voice->channel].volume
                                              ? midi_channels[channel].volume * message->velocity >> 7
                                              : message->velocity;

                        SET_ACTIVE_VOICE(voice_slot);
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
                for (int voice_slot = 0; voice_slot < MAX_MIDI_VOICES; ++voice_slot) {
                    const struct midi_voice_s *voice = &midi_voices[voice_slot];
                    if (IS_ACTIVE_VOICE(voice_slot) && voice->channel == channel && voice->note == message->note) {
                        if (channel == 9) {
                            CLEAR_ACTIVE_VOICE(voice_slot);
                        } else {
                            midi_voices[voice_slot].velocity /= 2;
                            midi_voices[voice_slot].release =
                                    midi_voices[voice_slot].sample_position + RELEASE_DURATION;
                        }
                        return;
                    }
                }
            break;


        case 0xB: // Controller Change
            switch (message->note) {
                case 0x7: // Volume change
                    debug_log("[MIDI] Channel %i volume %i\n", channel, midi_channels[channel].volume);
                    midi_channels[channel].volume = message->velocity;
                    for (int voice_slot = 0; voice_slot < MAX_MIDI_VOICES; ++voice_slot) {
                        if (midi_voices[voice_slot].channel == channel) {
                            midi_voices[voice_slot].velocity =
                                    message->velocity * midi_voices[voice_slot].velocity_base >> 7;
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

                        for (int voice_slot = 0; voice_slot < MAX_MIDI_VOICES; ++voice_slot)
                            if (midi_voices[voice_slot].channel == channel) {
                                if (channel == 9) {
                                    // midi_voices[voice_number].playing = 0;
                                    CLEAR_ACTIVE_VOICE(voice_slot);
                                } else {
                                    midi_voices[voice_slot].velocity /= 2;
                                    midi_voices[voice_slot].release =
                                            midi_voices[voice_slot].sample_position + RELEASE_DURATION;
                                }
                                // midi_voices[voice_number].playing = 0;
                                // CLEAR_ACTIVE_VOICE(voice_number);
                                // midi_voices[voice_number].release = 11025; // 1/4 seconds
                            }
                    }
                    debug_log("[MIDI] Channel %i sustain %i\n", channel, message->velocity);

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
                    memset(midi_channels, 0, sizeof(midi_channel_t) * MIDI_CHANNELS);
                    break;
                default:
                    debug_log("[MIDI] Unknown channel %i controller %02x %02x\n", channel, message->note,
                              message->velocity);
            }
            break;

        case 0xC: // Channel Program
            midi_channels[channel].program = message->note;

            for (int voice_slot = 0; voice_slot < MAX_MIDI_VOICES; ++voice_slot)
                if (midi_voices[voice_slot].channel == channel) {
                    CLEAR_ACTIVE_VOICE(voice_slot);
                }

            debug_log("[MIDI] Channel %i program %i\n", message->command & 0xf, message->note);
            break;
        case 0xE:
            const int pitch_bend = (message->velocity * 128 + message->note);
            const int cents = pitch_pows[pitch_bend];
            midi_channels[channel].pitch = cents;
            debug_log("[MIDI] Channel %i pitch_bend %i cents %i 44000->%i\n", channel, pitch_bend - 8192, cents,
                      apply_pitch(44000, cents));
            for (int voice_slot = 0; voice_slot < MAX_MIDI_VOICES; ++voice_slot)
                if (midi_voices[voice_slot].channel == channel) {
                    midi_voices[voice_slot].frequency_m100 = apply_pitch(
                        midi_voices[voice_slot].frequency_m100, cents);
                }
            break;
        default:
            debug_log("[MIDI] Unknown channel %i command %x message %04x \n", channel, message->command >> 4,
                      midi_command);
            break;
    }
}
