#include "emulator.h"

uint8_t crt_controller_idx, crt_controller[32];
uint8_t port60, port61, port64;
uint8_t cursor_start = 12, cursor_end = 13;
uint32_t vram_offset = 0x0;

static uint16_t adlibregmem[5], adlib_register = 0;
static uint8_t adlibstatus = 0;


void portout(uint16_t portnum, uint16_t value) {
    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            out8259(portnum, value);
            return;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43: //i8253
            out8253(portnum, value);
            break;
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
// Tandy 3 voice
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
            return tandy_write(portnum, value);
// GameBlaster / Creative Music System
        case 0x220:
        case 0x221:
        case 0x222:
        case 0x223:
            return cms_write(portnum, value);
// AdLib / OPL
        case 0x388: // adlib
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

            return adlib_write(adlib_register, value);
// EGA/VGA
        case 0x3C0:
        case 0x3C4:
        case 0x3C5:
        case 0x3C6:
        case 0x3C7:
        case 0x3C8:
        case 0x3C9:
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
                case 0x6:
                    // 160x100x16 mode TODO: Add more checks
                    if ((value == 0x64 && (videomode == 1 || videomode == 3))) {
                        videomode = 0x77;
                    }
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
//                    printf("vram offset %04X\n", vram_offset);
                    break;
            }

//            if ((crt_controller_idx != 0x0E) && (crt_controller_idx != 0x0F))
//                printf("CRT %x %x\n", crt_controller_idx, value);

            crt_controller[crt_controller_idx] = value;

            break;
        case 0x3D8:
        case 0x3D9:
            cga_portout(portnum, value);
            break;
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

// Alib
        case 0x388:
        case 0x389:
            if (!adlibregmem[4])
                adlibstatus = 0;
            else
                adlibstatus = 0x80;

            adlibstatus = adlibstatus + (adlibregmem[4] & 1) * 0x40 + (adlibregmem[4] & 2) * 0x10;
            return adlibstatus;

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
            return 0x00;
    }
}


void portout16(uint16_t portnum, uint16_t value) {
    portout(portnum, (uint8_t) value);
    portout(portnum + 1, (uint8_t) (value >> 8));
}

uint16_t portin16(uint16_t portnum) {
    return portin(portnum) | portin(portnum + 1) >> 8;
}
