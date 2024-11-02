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
    float frequency;
    uint8_t velocity;
    float sample_position;
} midi_channels[16] = {0};


// Precomputed frequency table for MIDI notes 0 to 127
static const float note_frequencies[128] = {
    8.176, 8.662, 9.177, 9.723, 10.301, 10.913, 11.562, 12.25, 12.978, 13.75, 14.568, 15.434, 16.352, 17.324, 18.354,
    19.445, 20.602, 21.827, 23.125, 24.5, 25.957, 27.5, 29.135, 30.868, 32.703, 34.648, 36.708, 38.891, 41.203, 43.654,
    46.249, 48.999, 51.913, 55, 58.27, 61.735, 65.406, 69.296, 73.416, 77.782, 82.407, 87.307, 92.499, 97.999, 103.826,
    110, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220,
    233.082, 246.942, 261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164,
    493.883, 523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767,
    1046.502, 1108.731, 1174.659, 1244.508, 1318.51, 1396.913, 1479.978, 1567.982, 1661.219, 1760, 1864.655, 1975.533,
    2093.005, 2217.461, 2349.318, 2489.016, 2637.02, 2793.826, 2959.955, 3135.963, 3322.438, 3520, 3729.31, 3951.066,
    4186.009, 4434.922, 4698.636, 4978.032, 5274.041, 5587.652, 5919.911, 6271.927, 6644.875, 7040, 7458.62, 7902.133,
    8372.018, 8869.844, 9397.273, 9956.063, 10548.082, 11175.303, 11839.822, 12543.854
};

int16_t midi_sample() {
    float sample = 0;
    for (int channel = 0; channel < 16; channel++)
        if (midi_channels[channel].playing) {
            sample += midi_channels[channel].velocity * sinf(
                6.283f * midi_channels[channel].frequency * midi_channels[channel].sample_position++ / SOUND_FREQUENCY);
        }

    return (sample * 32);
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
            channel->frequency = note_frequencies[message->note];
            channel->velocity = message->velocity;
            break;
        }
        case 0x8: // Note OFF
            channel->playing = 0;
            channel->velocity = message->velocity;
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
