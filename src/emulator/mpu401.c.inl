// https://midi.org/summary-of-midi-1-0-messages
// https://www.blitter.com/~russtopia/MIDI/~jglatt/tech/midispec/messages.htm
// https://newt.phys.unsw.edu.au/jw/notes.html
// https://massimo-nazaria.github.io/midi-synth.html
#include <emulator/emulator.h>
#define EMULATED_MIDI
// #define DEBUG_MPU401 1

enum { STATUS_READY = 0, STATUS_OUTPUT_NOT_READY = 0x40, STATUS_INPUT_NOT_READY = 0x80 };

uint8_t mpu_status = STATUS_INPUT_NOT_READY;
uint8_t mpu_rx_data = 0;

static int midi_pos, midi_len;
static uint32_t midi_command;
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static int midi_insysex;


#if defined(EMULATED_MIDI)
struct midi_channel_s {
    uint8_t playing;
    uint8_t note;
    int32_t frequency;
    uint8_t velocity;
    int32_t sample_position;
} midi_channels[16] = {0};

static const int32_t note_frequencies_m_100[128] = {
    818, 866, 918, 972, 1030, 1091, 1156, 1225, 1298, 1375, 1457, 1543,
    1635, 1732, 1835, 1945, 2060, 2183, 2312, 2450, 2596, 2750, 2914, 3087,
    3270, 3465, 3671, 3889, 4120, 4365, 4625, 4900, 5191, 5500, 5827, 6174,
    6541, 6930, 7342, 7778, 8241, 8731, 9250, 9800, 10383, 11000, 11654, 12347,
    13081, 13859, 14683, 15556, 16481, 17461, 18500, 19600, 20765, 22000, 23308, 24694,
    26163, 27718, 29366, 31113, 32963, 34923, 36999, 39200, 41530, 44000, 46616, 49388,
    52325, 55437, 58733, 62225, 65925, 69846, 73999, 78399, 83061, 88000, 93233, 98777,
    104650, 110873, 117466, 124451, 131851, 139691, 147998, 156798, 166122, 176000, 186466, 197553,
    209300, 221746, 234932, 248902, 263702, 279383, 295996, 313596, 332244, 352000, 372931, 395107,
    418601, 443492, 469864, 497803, 527404, 558765, 591991, 627193, 664488, 704000, 745862, 790213,
    837202, 886984, 939727, 995606, 1054808, 1117530, 1183982, 1254385
};

inline int16_t sin100sf_m_64(int32_t a) {
    const static float m = 6.283f / (100.0f * SOUND_FREQUENCY);
    return 32 * sinf(a * m);
}

int16_t midi_sample() {
    int16_t sample = 0;
    for (int channel = 0; channel < 16; ++channel) {
        if (midi_channels[channel].playing) {
            sample += midi_channels[channel].velocity * sin100sf_m_64(midi_channels[channel].frequency * midi_channels[channel].sample_position++);
        }
    }
    return sample;
}

static INLINE void parse_midi(uint32_t midi_command) {
    struct {
        uint8_t command;
        uint8_t note;
        uint8_t velocity;
        uint8_t other;
    } *message = &midi_command;
    struct midi_channel_s *channel = &midi_channels[message->command & 0xf];
    switch (message->command >> 4) {
        case 0x9: {
            // Note ON
            channel->playing = 1;
            channel->sample_position = 0;
            channel->note = message->note;
            channel->frequency = note_frequencies_m_100[message->note];
            channel->velocity = message->velocity;
            if ((message->command & 0xf) == 9) {
                channel->velocity =message->velocity  * 2;
            }
            break;
        }
        case 0x8: // Note OFF
            channel->playing = 0;
            // channel->velocity = message->velocity;
            break;
        default:
#ifdef DEBUG_MPU401
             printf("Unknown command %x message %04x \n", message->command >> 4, midi_command);
#endif
            break;
    }
    // printf("%x %x %x %x %x\n", message->command, message->note, message->velocity, message->other, midi_command );
}

#else
static int midi_id = 0;
static HMIDIOUT midi_out_device = NULL;

static INLINE void midi_init_once() {
    if (midi_out_device != NULL) return;

    MMRESULT hr = midiOutOpen(&midi_out_device, midi_id, 0, 0, CALLBACK_NULL);
    if (hr != MMSYSERR_NOERROR) {
#ifdef DEBUG_MPU401
        printf("midiOutOpen error - %08X\n", hr);
#endif
        return;
    }
    midiOutReset(midi_out_device);
}

static INLINE void midi_close() {
    if (midi_out_device != NULL) {
        midiOutReset(midi_out_device);
        midiOutClose(midi_out_device);
        midi_out_device = NULL;
    }
}

static uint8_t midi_sysex_data[4096 + 2];
static INLINE void midi_send_sysex() {
    MIDIHDR hdr = {
        .lpData = (LPSTR) midi_sysex_data,
        .dwBufferLength = midi_pos,
        .dwFlags = 0
    };
    // Prepare the MIDI header for sending
    MMRESULT result = midiOutPrepareHeader(midi_out_device, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
#ifdef DEBUG_MPU401
        printf("Error preparing SysEx header: %08X\n", result);
#endif
        return;
    }

    // Send the SysEx message
    result = midiOutLongMsg(midi_out_device, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
#ifdef DEBUG_MPU401
        printf("Error sending SysEx message: %08X\n", result);
#endif
        midiOutUnprepareHeader(midi_out_device, &hdr, sizeof(MIDIHDR));
        return;
    }

    // Wait for the message to be sent
    while (!(hdr.dwFlags & MHDR_DONE)) {
        Sleep(1); // Allow time for the message to finish sending
    }

    // Unprepare the header after sending
    result = midiOutUnprepareHeader(midi_out_device, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
#ifdef DEBUG_MPU401
        printf("Error unpreparing SysEx header: %08X\n", result);
#endif
    }

    // Reset the SysEx state
    midi_insysex = 0;
}
int16_t midi_sample() { return 0; }


#endif


static INLINE uint8_t mpu401_read(uint16_t portnum) {
    if (portnum & 1) {
        return mpu_status;
    }

    mpu_status = STATUS_INPUT_NOT_READY;
    return mpu_rx_data;
}

static INLINE void mpu401_write(uint16_t portnum, uint8_t value) {
    if (portnum & 1) {
        switch (value) {
            case 0xDF:
                mpu_rx_data = 0xFE; // Ack
#if !defined(EMULATED_MIDI)
                midi_send_sysex();
#endif
                break;
            case 0xD0: // Enter UART Mode
                mpu_rx_data = 0xFF; // ???
                break;
            case 0x3F: // Enter Intellegent UART mode
                mpu_rx_data = 0xFE; // Ack
                break;
            default:
#ifdef DEBUG_MPU401
                printf("[MIDI] Unknown %x %x\n", portnum, value);
#endif
            case 0xFF: // Reset
#if !defined(EMULATED_MIDI)
                midi_init_once();
#endif
                mpu_rx_data = 0xFE; // Ack
                break;
        }

        mpu_status = STATUS_READY;
        return;
    }

    if (value & 0x80 && !(value == 0xF7 && midi_insysex)) {
        midi_pos = 0;
        midi_len = midi_lengths[value >> 4 & 7];
        midi_command = 0;

        if (value == 0xF0) midi_insysex = 1;
    }

    if (midi_insysex) {
#if !defined(EMULATED_MIDI)
        midi_sysex_data[midi_pos++] = value;

        if (value == 0xF7 || midi_pos >= sizeof(midi_sysex_data)) {
            midi_send_sysex();
        }
#endif
        if (value == 0xF7) {
            midi_insysex = 0;
        }
        return;
    }

    if (midi_len) {
        midi_command |= value << midi_pos * 8;
        if (++midi_pos == midi_len) {
#if defined(EMULATED_MIDI)
            parse_midi(midi_command);
#else
            midiOutShortMsg(midi_out_device, midi_command);
#endif
        }
    }
}
