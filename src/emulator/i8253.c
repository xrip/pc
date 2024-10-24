#include "emulator.h"
#define PIT_MODE_LATCHCOUNT  0
#define PIT_MODE_LOBYTE 1
#define PIT_MODE_HIBYTE 2
#define PIT_MODE_TOGGLE 3

struct i8253_s i8253 = { 0 };
int speakerenabled = 0;
int timer_period = 54925;

void init8253() {
    memset(&i8253, 0, sizeof(i8253));
}

void out8253(uint16_t portnum, uint8_t value) {
    uint8_t curbyte = 0;
    portnum &= 3;
    switch (portnum) {
        case 0:
        case 1:
        case 2: //channel data
            if ((i8253.accessmode[portnum] == PIT_MODE_LOBYTE) ||
                ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0))) {
                curbyte = 0;
            } else if ((i8253.accessmode[portnum] == PIT_MODE_HIBYTE) ||
                       ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1))) {
                curbyte = 1;
            }

            if (curbyte == 0) {
                //low byte
                i8253.chandata[portnum] = (i8253.chandata[portnum] & 0xFF00) | value;
            } else {
                //high byte
                i8253.chandata[portnum] = (i8253.chandata[portnum] & 0x00FF) | ((uint16_t) value << 8);
            }

            if (i8253.chandata[portnum] == 0) {
                i8253.effectivedata[portnum] = 65536;
#if !PICO_ON_DEVICE || !I2S_SOUND

                tandy_write(0xff, 0b10011111);
#endif
                speakerenabled = 0;
            } else {
                i8253.effectivedata[portnum] = i8253.chandata[portnum];

                if (port61 & 2) {
#if !PICO_ON_DEVICE || !I2S_SOUND
//                 uint16_t tone = (3579545 / (32 * 1193182 / i8253.effectivedata[portnum])) - 1;
#if PICO_ON_DEVICE
                    uint16_t tone = (__fast_mul( i8253.effectivedata[portnum] >> 20, 111860)) - 1;
#else
                    uint16_t tone = (111860 * i8253.effectivedata[portnum] >> 20) - 1;
#endif
                   tandy_write(0xff, 0x80 | (tone & 0x0F));
                   tandy_write(0xff, (tone >> 4) & 0x3F);
                   tandy_write(0xff, 0b10010000);
#endif
                    speakerenabled = 1; // set 50% (127) duty cycle ==> Sound output on
                } else {
#if !PICO_ON_DEVICE || !I2S_SOUND
                    tandy_write(0xff, 0b10011111);
#endif
                    speakerenabled = 0; // set 0% (0) duty clcle ==> Sound output off
                }
            }


            i8253.active[portnum] = 1;
            if (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) {
                i8253.bytetoggle[portnum] = (~i8253.bytetoggle[portnum]) & 1;
            }

            i8253.chanfreq[portnum] = 1193182 / i8253.effectivedata[portnum];
#if 1
            if (portnum == 0) {
                // Timer freq 1,193,180
#if PICO_ON_DEVICE
                timer_period =  1000000 / i8253.chanfreq[portnum];
#else
                timer_period =  i8253.chanfreq[portnum];
#endif
            }
#endif
            break;
        case 3: //mode/command
            i8253.accessmode[value >> 6] = (value >> 4) & 3;
            if (i8253.accessmode[value >> 6] == PIT_MODE_TOGGLE) i8253.bytetoggle[value >> 6] = 0;
            break;
    }
}

uint8_t in8253(uint16_t portnum) {
    uint8_t curbyte;
    portnum &= 3;
    switch (portnum) {
        case 0:
        case 1:
        case 2: //channel data
            if ((i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_LOBYTE) ||
                ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0))) {
                curbyte = 0;
            } else if ((i8253.accessmode[portnum] == PIT_MODE_HIBYTE) ||
                       ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1))) {
                curbyte = 1;
            }

            if ((i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_TOGGLE))
                i8253.bytetoggle[portnum] = (~i8253.bytetoggle[portnum]) & 1;
            if (curbyte == 0) {
                //low byte
                if (i8253.counter[portnum] < 10) i8253.counter[portnum] = i8253.chandata[portnum];
                i8253.counter[portnum] -= 10;
                return ((uint8_t) i8253.counter[portnum]);
            } else {
                //high byte
                return ((uint8_t) (i8253.counter[portnum] >> 8));
            }
    }
    return (0);
}

