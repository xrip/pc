#include <emulator/emulator.h>
#include <windows.h>

enum { STATUS_READY = 0, STATUS_OUTPUT_NOT_READY = 0x40, STATUS_INPUT_NOT_READY = 0x80 };

uint8_t mpu_status = STATUS_INPUT_NOT_READY;
uint8_t mpu_rx_data = 0;
static int midi_id = 0;
static HMIDIOUT midi_out_device = NULL;

static INLINE void midi_init_once() {
    static int initialized = 0;
    if (initialized) return;

    MMRESULT hr = midiOutOpen(&midi_out_device, midi_id, 0, 0, CALLBACK_NULL);
    if (hr != MMSYSERR_NOERROR) {
        printf("midiOutOpen error - %08X\n", hr);
        return;
    }
    midiOutReset(midi_out_device);
    initialized = 1;
}

static INLINE void midi_close() {
    if (midi_out_device != NULL) {
        midiOutReset(midi_out_device);
        midiOutClose(midi_out_device);
        midi_out_device = NULL;
    }
}

static int midi_pos, midi_len;
static uint32_t midi_command;
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static int midi_insysex;
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
        printf("Error preparing SysEx header: %08X\n", result);
        return;
    }

    // Send the SysEx message
    result = midiOutLongMsg(midi_out_device, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        printf("Error sending SysEx message: %08X\n", result);
        midiOutUnprepareHeader(midi_out_device, &hdr, sizeof(MIDIHDR));
        return;
    }

    // Wait for the message to be sent
    while (hdr.dwFlags & MHDR_DONE == 0) {
        Sleep(1); // Allow time for the message to finish sending
    }

    // Unprepare the header after sending
    result = midiOutUnprepareHeader(midi_out_device, &hdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        printf("Error unpreparing SysEx header: %08X\n", result);
    }

    // Reset the SysEx state
    midi_insysex = 0;
}


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
            case 0x3F: // Enter UART mode
                mpu_rx_data = 0xFE; // Ack
                mpu_status = STATUS_READY;
                break;
            default:
                printf("[MIDI] Unknown %x %x\n", portnum, value);
            case 0xFF: // Reset
                midi_init_once();
                mpu_rx_data = 0xFE; // Ack
                mpu_status = STATUS_READY;
                break;
        }
        return;
    }

    if (value & 0x80 && !(value == 0xF7 && midi_insysex)) {
        midi_pos = 0;
        midi_len = midi_lengths[value >> 4 & 7];
        midi_command = 0;

        if (value == 0xF0) midi_insysex = 1;
    }

    if (midi_insysex) {
        midi_sysex_data[midi_pos++] = value;

        if (value == 0xF7 || midi_pos >= sizeof(midi_sysex_data)) {
            midi_send_sysex();
        }

        return;
    }

    if (midi_len) {
        midi_command |= value << midi_pos * 8;

        if (++midi_pos == midi_len) {
            midiOutShortMsg(midi_out_device, midi_command);
        }
    }
}
