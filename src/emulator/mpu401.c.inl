#include <emulator/emulator.h>
enum { STATUS_READY = 0, STATUS_OUTPUT_NOT_READY = 0x40, STATUS_INPUT_NOT_READY = 0x80 };

uint8_t mpu_status = STATUS_INPUT_NOT_READY;
uint8_t mpu_rx_data  = 0;
static int midi_id = 0;
static HMIDIOUT midi_out_device = NULL;

static INLINE void midi_init() {
    printf("calling midi_init()\n");
    MMRESULT hr = midiOutOpen(&midi_out_device, midi_id, 0, 0, CALLBACK_NULL);
    if (hr != MMSYSERR_NOERROR) {
        printf("1 midiOutOpen error - %08X\n", hr);
        midi_id = 0;
        hr = midiOutOpen(&midi_out_device, midi_id, 0, 0, CALLBACK_NULL);
        if (hr != MMSYSERR_NOERROR) {
            printf("2 midiOutOpen error - %08X\n", hr);
            return;
        }
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

static INLINE uint8_t midi_read(uint16_t portnum) {
    if (portnum & 1) {
        return mpu_status;
    }

    mpu_status = STATUS_INPUT_NOT_READY;
    return mpu_rx_data;
}

static int midi_pos, midi_len;
static uint32_t midi_command;
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static int midi_insysex;
static uint8_t midi_sysex_data[1024 + 2];

static INLINE void midi_send_sysex() {
    MIDIHDR hdr;

    hdr.lpData = (LPSTR)midi_sysex_data;
    hdr.dwBufferLength = midi_pos;
    hdr.dwFlags = 0;

    /*        pclog("Sending sysex : ");
            for (c = 0; c < midi_pos; c++)
                    pclog("%02x ", midi_sysex_data[c]);
            pclog("\n");*/

    midiOutPrepareHeader(midi_out_device, &hdr, sizeof(MIDIHDR));
    midiOutLongMsg(midi_out_device, &hdr, sizeof(MIDIHDR));

    midi_insysex = 0;
}
static INLINE void midi_write(uint16_t portnum, uint8_t value) {

    if (portnum & 1) {
        switch (value) {
            case 0xFF: /*{ // Reset
                mpu_rx_data = 0xFE; // Ack
                mpu_status = STATUS_READY;
                break;
            }
            */
            case 0x3F: { // Enter UART mode
                mpu_rx_data = 0xFE; // Ack
                mpu_status = STATUS_READY;
                break;
            }
            default:
                printf("[MIDI] Unknown %x %x\n",portnum, value);
                break;
        }
        return;
    }
    static int once  =0;
    if (!once) {
        midi_init();
        once = 1;
    }
    // printf("[MIDI Output Writing] %x %x\n", portnum, value);
    if ((value & 0x80) && !(value == 0xf7 && midi_insysex)) {
        midi_pos = 0;
        midi_len = midi_lengths[(value >> 4) & 7];
        midi_command = 0;
        if (value == 0xf0)
            midi_insysex = 1;
    }

    if (midi_insysex) {
        midi_sysex_data[midi_pos++] = value;

        if (value == 0xf7 || midi_pos >= 1024 + 2)
            midi_send_sysex();
        return;
    }

    if (midi_len) {
        midi_command |= (value << (midi_pos * 8));

        midi_pos++;

        if (midi_pos == midi_len)
            midiOutShortMsg(midi_out_device, midi_command);
    }
}