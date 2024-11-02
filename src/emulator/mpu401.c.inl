// https://midi.org/summary-of-midi-1-0-messages
// https://www.blitter.com/~russtopia/MIDI/~jglatt/tech/midispec/messages.htm
// https://newt.phys.unsw.edu.au/jw/notes.html
// https://massimo-nazaria.github.io/midi-synth.html
#include <math.h>
#include <emulator/emulator.h>
#define EMULATED_MIDI
#define DEBUG_MPU401 1

enum { STATUS_READY = 0, STATUS_OUTPUT_NOT_READY = 0x40, STATUS_INPUT_NOT_READY = 0x80 };

uint8_t mpu_status = STATUS_INPUT_NOT_READY;
uint8_t mpu_rx_data = 0;

static int midi_pos, midi_len;
static uint32_t midi_command;
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static int midi_insysex;


#if defined(EMULATED_MIDI)
struct {
    uint8_t playing;
    uint8_t note;
    uint8_t velocity;
    float sample_position;
} midi_channels[16] = {0};


// Precomputed frequency table for MIDI notes 0 to 127
static const float note_frequencies[128] = {
    8.18, 8.66, 9.18, 9.72, 10.30, 10.91, 11.56, 12.25, 12.98, 13.75, 14.57, 15.43,
    16.35, 17.32, 18.35, 19.45, 20.60, 21.83, 23.12, 24.50, 25.96, 27.50, 29.14, 30.87,
    32.70, 34.65, 36.71, 38.89, 41.20, 43.65, 46.25, 49.00, 51.91, 55.00, 58.27, 61.74,
    65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47,
    130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
    261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
    523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77,
    1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760.00, 1864.66, 1975.53,
    2093.00, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07,
    4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040.00, 7458.62, 7902.13,
    8372.02, 8869.84, 9397.27, 9956.06, 10548.08, 11175.30, 11839.82, 12543.85
};

int16_t midi_sample() {
    float sample = 0;
    for (int channel = 0; channel < 16; channel++)
        if (midi_channels[channel].playing) {
            const float frequency = note_frequencies[midi_channels[channel].note];

            sample += midi_channels[channel].velocity * sinf(6.283f * frequency * midi_channels[channel].sample_position++ / SOUND_FREQUENCY);
        }

    return (sample * 4);
}

static INLINE void parse_midi(uint32_t midi_command) {
    struct {
        uint8_t command;
        uint8_t note;
        uint8_t velocity;
        uint8_t other;
    } *message = &midi_command;
    switch (message->command >> 4) {
        case 0x9: {
            // Note ON
            uint8_t channel = message->command & 0xf;
            midi_channels[channel].playing = 1;
            midi_channels[channel].sample_position = 0;
            midi_channels[channel].note = message->note;
            midi_channels[channel].velocity = message->velocity;
            break;
        }
        case 0x8: // Note OFF
            uint8_t channel = message->command & 0xf;
            midi_channels[channel].playing = 0;
            midi_channels[channel].velocity = message->velocity;
            break;
        default:
            // printf("Unknown command %x message %04x \n", message->command, message->raw);


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
