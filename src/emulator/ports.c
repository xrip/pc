#include <time.h>
#include "emulator.h"
#if PICO_ON_DEVICE
#include "nespad.h"
#endif

uint8_t crt_controller_idx, crt_controller[32];
uint8_t port60, port61, port64;
uint8_t cursor_start = 12, cursor_end = 13;
uint32_t vram_offset = 0x0;

#if !PICO_ON_DEVICE
static uint16_t adlibregmem[5], adlib_register = 0;
static uint8_t adlibstatus = 0;

#endif
static inline uint8_t rtc_read(uint16_t addr) {
    uint8_t ret = 0xFF;
    struct tm tdata;

    time(&tdata);
    struct tm *t = localtime(&tdata);

    t->tm_year = 24;
    addr &= 0x1F;
    switch (addr) {
        case 1:
            ret = 0;
            break;
        case 2:
            ret = (uint8_t)t->tm_sec;
            break;
        case 3:
            ret = (uint8_t)t->tm_min;
            break;
        case 4:
            ret = (uint8_t)t->tm_hour;
            break;
        case 5:
            ret = (uint8_t)t->tm_wday;
            break;
        case 6:
            ret = (uint8_t)t->tm_mday;
            break;
        case 7:
            ret = (uint8_t)t->tm_mon + 1;
            break;
        case 9:
            ret = (uint8_t)t->tm_year % 100;
            break;
    }

    if (ret != 0xFF) {
        uint8_t rh, rl;
        rh = (ret / 10) % 10;
        rl = ret % 10;
        ret = (rh << 4) | rl;
    }

    return ret;
}

uint16_t gamepad_tick = 0;
uint8_t gamepad_in() {
    //printf("gamepad in %d \n", gamepad_tick);
    gamepad_tick++;
    if (gamepad_tick >= 32) {
        gamepad_tick = 0;
        return 0b11110000;
    }
#if PICO_ON_DEVICE
    nespad_read();
    nespad_state = ~nespad_state;
    uint8_t gamepad_state  = ((nespad_state & DPAD_RIGHT)) != 0;
    gamepad_state |= ((nespad_state & DPAD_UP)     != 0) << 1;
    gamepad_state |= ((nespad_state & DPAD_LEFT)   != 0) << 2;
    gamepad_state |= ((nespad_state & DPAD_DOWN)   != 0) << 3;

    gamepad_state |= ((nespad_state & DPAD_A)      != 0) << 4;
    gamepad_state |= ((nespad_state & DPAD_B)      != 0) << 5;
    gamepad_state |= ((nespad_state & DPAD_START)  != 0) << 6;
    gamepad_state |= ((nespad_state & DPAD_SELECT) != 0) << 7;

    return gamepad_state & 0xff;
#else

    //printf("read \r\n");
#endif
}

static inline void gamepad_out() {
    gamepad_tick = 0;
}


void portout(uint16_t portnum, uint16_t value) {
    switch (portnum) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
            return i8237_writeport(portnum, value);
        case 0x20:
        case 0x21: //i8259
            return out8259(portnum, value);
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43: //i8253
            return out8253(portnum, value);
        case 0x61:
            port61 = value;
            if ((value & 3) == 3) {
                tandy_write(0, 0b10010000);
                speakerenabled = 1;
            } else {
                tandy_write(0, 0b10011111);
                speakerenabled = 0;
            }

            break;
        case 0x64:
            port64 = value;
            break;
// i8237 DMA
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x87:
            return i8237_writepage(portnum, value);
// Tandy 3 voice
        case 0x1E0:
        case 0x2C0:
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
            sn76489_out(value);
            return tandy_write(portnum, value);
// Gamepad
        case 0x201:
            return gamepad_out();
// GameBlaster / Creative Music System
        case 0x220:
        case 0x221:
        case 0x222:
        case 0x223:
        case 0x224:
        case 0x225:
        case 0x226:
        case 0x227:
        case 0x228:
        case 0x229:
        case 0x22a:
        case 0x22b:
        case 0x22c:
        case 0x22d:
        case 0x22e:
        case 0x22f:
            blaster_write(portnum, value);
            cms_out(portnum, value);
            return cms_write(portnum, value);

        case 0x278:
            return covox_out(portnum, value);

        case 0x378:
//            return covox_out(portnum, value);
        case 0x37A:
            return dss_out(portnum, value);
// AdLib / OPL
#if !PICO_ON_DEVICE
        case 0x388:
            adlib_register = value;
            break;
        case 0x389:
            if (adlib_register <= 4) {
                adlibregmem[adlib_register] = value;

                if (adlib_register == 4 && value & 0x80) {
                    adlibstatus = 0;
                    adlibregmem[4] = 0;
                }
            }
            return adlib_write_d(adlib_register, value);
#else
        case 0x388:
        case 0x389:
            return outadlib(portnum, value);
#endif
// EGA/VGA
        case 0x3C0:
        case 0x3C4:
        case 0x3C5:
        case 0x3C6:
        case 0x3C7:
        case 0x3C8:
        case 0x3C9:
        case 0x3CE:
        case 0x3CF:
            return vga_portout(portnum, value);
// https://stanislavs.org/helppc/6845.html
// https://bitsavers.trailing-edge.com/components/motorola/_dataSheets/6845.pdf
// https://www.theoddys.com/acorn/the_6845_crtc/the_6845_crtc.html
// MC6845 CRTC
        case 0x3D4:
            crt_controller_idx = value & 31;
            break;
        case 0x3D5:
            switch (crt_controller_idx) {
                case 0x4: {
                    if (value == 0x3e/* && videomode == 1*/) {
                        videomode = 0x79;

                    }
                    break;
                }

                case 0x6:
                    //printf("!!! Y = %i\n", value);
                    // 160x100x16 or 160x200x16 mode TODO: Add more checks
                    if (value == 0x64 && (videomode <= 3)) {
                        videomode = cga_hires ? 0x76 : 0x77;
                    }

                    // 160x46x16 mode TODO: Add more checks
                    if (value == 0x2e/* && (videomode <= 3))*/) {
                        videomode = 0x87;
                    }
                    break;

                case 0x8:
                    break;
// Cursor pos
                case 0x0A:
                    cursor_start = value;
                    break;
                case 0x0B:
                    cursor_end = value;
                    break;

// Screen offset
                case 0x0C: // Start address (MSB)
                    vram_offset = value;
                    break;
                case 0x0D: // Start address (LSB)
                    vram_offset = (uint32_t) vram_offset << 8 | (uint32_t) value;
                    //printf("vram offset %04X\n", vram_offset);
                    break;
            }

//            if ((crt_controller_idx != 0x03) && ((crt_controller_idx != 0x0E) && (crt_controller_idx != 0x0F) && (crt_controller_idx != 0x0c) && (crt_controller_idx != 0x0d)))
//                printf("CRT %x %x\n", crt_controller_idx, value);

            crt_controller[crt_controller_idx] = value;

            break;
        case 0x3D8:
        case 0x3D9:
            return cga_portout(portnum, value);
        case 0x3DA:
        case 0x3DE:
        case 0x3DF:
            return tga_portout(portnum, value);
        case 0x3F8:
        case 0x3F9:
        case 0x3FA:
        case 0x3FB:
        case 0x3FC:
        case 0x3FD:
        case 0x3FE:
        case 0x3FF:
            return mouse_portout(portnum, value);
    }
}

uint16_t portin(uint16_t portnum) {
    switch (portnum) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
            return i8237_readport(portnum);
        case 0x20:
        case 0x21: //i8259
            return in8259(portnum);
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43: //i8253
            return in8253(portnum);

// Keyboard
        case 0x60:
            return port60;
        case 0x61:
            return port61;
        case 0x64:
            return port64;
// i8237 DMA
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x87:
            return i8237_readpage(portnum);

        case 0x201:
            return gamepad_in();

        case 0x220:
        case 0x221:
        case 0x222:
        case 0x223:
        case 0x224:
        case 0x225:
        case 0x226:
        case 0x227:
        case 0x228:
        case 0x229:
        case 0x22a:
        case 0x22b:
        case 0x22c:
        case 0x22d:
        case 0x22e:
        case 0x22f:
            return blaster_read(portnum);
            return cms_in(portnum);
// RTC
        case 0x240:
        case 0x241:
        case 0x242:
        case 0x243:
        case 0x244:
        case 0x245:
        case 0x246:
        case 0x247:
        case 0x248:
        case 0x249:
        case 0x24A:
        case 0x24B:
        case 0x24C:
        case 0x24D:
        case 0x24E:
        case 0x24F:
        case 0x250:
        case 0x251:
        case 0x252:
        case 0x253:
        case 0x254:
        case 0x255:
        case 0x256:
        case 0x257:
            return rtc_read(portnum);
        case 0x27A: // LPT2 status (covox is always ready)
            return 0;
        case 0x379:
            return dss_in(portnum);
// Adlib
        case 0x388:
        case 0x389:
#if !PICO_ON_DEVICE
            if (!adlibregmem[4])
                adlibstatus = 0;
            else
                adlibstatus = 0x80;

            adlibstatus = adlibstatus + (adlibregmem[4] & 1) * 0x40 + (adlibregmem[4] & 2) * 0x10;
            return adlibstatus;
#else
            return inadlib(portnum);
#endif
        case 0x3C1:
        case 0x3C2:
        case 0x3C7:
        case 0x3C8:
        case 0x3C9:
            return vga_portin(portnum);

        case 0x3D4:
            return crt_controller_idx;
        case 0x3D5:
            return crt_controller[crt_controller_idx];
        case 0x3DA:
            return cga_portin(portnum);
        case 0x3F8:
        case 0x3F9:
        case 0x3FA:
        case 0x3FB:
        case 0x3FC:
        case 0x3FD:
        case 0x3FE:
        case 0x3FF:
            return mouse_portin(portnum);
        default:
            return 0xFF;
    }
}


void portout16(uint16_t portnum, uint16_t value) {
    portout(portnum, (uint8_t) value);
    portout(portnum + 1, (uint8_t) (value >> 8));
}

uint16_t portin16(uint16_t portnum) {
    return portin(portnum) | portin(portnum + 1) >> 8;
}
