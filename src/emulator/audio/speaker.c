#include "emulator/emulator.h"


inline int16_t speaker_sample() {
    if (!speakerenabled) return 0;
    static uint32_t speakerfullstep, speakerhalfstep, speakercurstep = 0;
    int16_t speakervalue;
    speakerfullstep = SOUND_FREQUENCY / i8253.chanfreq[2];
    if (speakerfullstep < 2)
        speakerfullstep = 2;
    speakerhalfstep = speakerfullstep >> 1;
    if (speakercurstep < speakerhalfstep) {
        speakervalue = 4096;
    } else {
        speakervalue = -4096;
    }
    speakercurstep = (speakercurstep + 1) % speakerfullstep;
    return speakervalue;
}