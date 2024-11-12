//https://the.earth.li/~tfm/oldpage/sb_dsp.html
#include "../emulator.h"

#define debug_log(...) printf(__VA_ARGS__)

uint16_t timeconst = 22;
uint64_t sb_samplerate = 22050;

#define BLASTER_IRQ 5
#define BLASTER_DMA_CHANNEL 1


// Sound Blaster DSP I/O port offsets
#define DSP_RESET           0x6
#define DSP_READ            0xA
#define DSP_WRITE           0xC
#define DSP_WRITE_STATUS    0xC
#define DSP_READ_STATUS     0xE

// Sound Blaster DSP commands.
#define DSP_DMA_HS_SINGLE       0x91
#define DSP_DMA_HS_AUTO         0x90
#define DSP_DMA_ADPCM           0x7F    //creative ADPCM 8bit to 3bit
#define DSP_DMA_SINGLE          0x14    //follosed by length
#define DSP_DMA_AUTO            0X1C    //length based on 48h
#define DSP_DMA_BLOCK_SIZE      0x48    //block size for highspeed/dma
//#define DSP_DMA_DAC 0x14
#define DSP_DIRECT_DAC          0x10
#define DSP_DIRECT_ADC          0x20
#define DSP_MIDI_READ_POLL      0x30
#define DSP_MIDI_WRITE_POLL     0x38
#define DSP_SET_TIME_CONSTANT   0x40
#define DSP_DMA_PAUSE           0xD0
#define DSP_DMA_PAUSE_DURATION  0x80    //Used by Tryrian
#define DSP_ENABLE_SPEAKER      0xD1
#define DSP_DISABLE_SPEAKER     0xD3
#define DSP_DMA_RESUME          0xD4
#define DSP_SPEAKER_STATUS      0xD8
#define DSP_IDENTIFICATION      0xE0
#define DSP_VERSION             0xE1
#define DSP_WRITETEST           0xE4
#define DSP_READTEST            0xE8
#define DSP_SINE                0xF0
#define DSP_IRQ                 0xF2
#define DSP_CHECKSUM            0xF4


#define BLASTER_BUFFER_LENGTH 16

typedef struct blaster_t {
    uint8_t buffer[BLASTER_BUFFER_LENGTH]; // FILO buffer
    uint8_t buffer_length;

    uint8_t latched_command;

    uint8_t speaker_enabled;
    uint8_t dma_enabled;

    uint16_t dma_length;

    uint8_t dma_pause_duration;
} blaster_t;

blaster_t blaster = {0};

inline void blaster_write_buffer(const uint8_t value) {
    if (blaster.buffer_length < BLASTER_BUFFER_LENGTH) blaster.buffer[blaster.buffer_length++] = value & 0xFF;
}

inline uint8_t blaster_read_buffer() {
    if (blaster.buffer_length == 0) return 0;

    const uint8_t result = blaster.buffer[0];

    for (int i = 0; i < blaster.buffer_length - 1; i++) {
        blaster.buffer[i] = blaster.buffer[i + 1];
    }

    blaster.buffer_length--;

    return result;
}

void blaster_reset() {
    blaster.speaker_enabled = 0;
    blaster.buffer_length = 0;
    blaster.latched_command = 0;
    blaster_write_buffer(0xAA);
}

void blaster_command(const uint8_t value) {
    static uint8_t latch_next_data = 0;
    // debug_log("[SB] Blaster data: 0x%02X\n", value);

    if (blaster.latched_command) {
        switch (blaster.latched_command) {
            case DSP_DMA_PAUSE_DURATION:
                if (!latch_next_data) {
                    blaster.dma_pause_duration = value;
                    latch_next_data = 1;
                    return;
                }
                blaster.dma_pause_duration |= value << 8;

                latch_next_data = 0;
                debug_log("[SB] DSP_DMA_PAUSE_DURATION: %ш\n", blaster.dma_pause_duration);
                break;
            case DSP_DMA_SINGLE:
                if (!latch_next_data) {
                    blaster.dma_length = value;
                    latch_next_data = 1;
                    return;
                }
                latch_next_data = 0;
                blaster.dma_length |= value << 8;
                debug_log("DSP_DMA_SINGLE %04X\n", blaster.dma_length);
                // blaster.dma_length++;
                blaster.dma_enabled = 1;
                break;
            case DSP_SET_TIME_CONSTANT:
                timeconst = 256 - value;
                sb_samplerate = 1000000 / timeconst;
                debug_log("DSP_SET_TIME_CONSTANT %i SAMPLE RATE %iHz\n", timeconst, sb_samplerate);
                break;
            case DSP_IDENTIFICATION:
                blaster_write_buffer(~value & 0xFF);
                debug_log("DSP_IDENTIFICATION 0x%02x\n", ~value & 0xFF);
                break;
            default:
                debug_log("Unknown command %02x value %02x\n", blaster.latched_command, value);
                break;
        }
        blaster.latched_command = 0;
        return;
    }

    switch (value) {
        case DSP_DMA_RESUME:
            debug_log("DSP_DMA_RESUME\n");
            blaster.dma_enabled = 1;
            blaster.latched_command = 0;
            break;

        case DSP_DMA_PAUSE:
            debug_log("DSP_DMA_PAUSE\n");
            blaster.dma_enabled = 0;
            blaster.latched_command = 0;
            break;
        case DSP_ENABLE_SPEAKER:
            // debug_log("DSP_ENABLE_SPEAKER\n");
            blaster.speaker_enabled = 1;
            blaster.latched_command = 0;
            break;
        case DSP_DISABLE_SPEAKER:
            // debug_log("DSP_DISABLE_SPEAKER\n");
            blaster.speaker_enabled = 0;
            blaster.latched_command = 0;
            break;

        case DSP_VERSION:
            // debug_log("DSP_VERSION\n");
            if (!latch_next_data) {
                latch_next_data = 1;
                blaster_write_buffer(2);
                blaster_write_buffer(1);
            } else {
                latch_next_data = 0;
            }
            blaster.latched_command = 0;
            break;

        case DSP_IRQ:
            doirq(BLASTER_IRQ);
            // debug_log("DSP_IRQ\n");
            blaster.latched_command = 0;
            break;
        default:
            blaster.latched_command = value;
            // debug_log("[SB] Multibyte command 0x%02x\n", value);
            break;
    }
}

void blaster_write(const uint16_t portnum, const uint8_t value) {
    printf("Blaster write %x %x\n", portnum, value);
    switch (portnum & 0xF) {
        case DSP_RESET:
            blaster_reset(value);
            break;
        case DSP_WRITE:
            blaster_command(value);
            break;
    }
}

uint8_t blaster_read(const uint16_t portnum) {
    printf("Blaster read %x\n", portnum);
    switch (portnum & 0xF) {
        case DSP_READ:
            return blaster_read_buffer();
        case DSP_READ_STATUS:
            return blaster.buffer_length ? 0x80 : 0x00;
        case DSP_WRITE_STATUS:
            return blaster.latched_command << 7;
        default:
            return 0xFF;
    }
}

int16_t blaster_sample() {
    if (blaster.dma_enabled) {
        if (blaster.dma_length--) {
            const uint8_t sample = i8237_read(BLASTER_DMA_CHANNEL);
            return blaster.speaker_enabled ? ((sample - 128) << 8) : 0;
        }
        // single
        blaster.dma_enabled = 0;
        blaster.dma_length = 0;
        doirq(BLASTER_IRQ); // fire irq dma sb dma transfer ended
        printf("SB IRQ\n");
        return 0;
    }
    return 0;
}
