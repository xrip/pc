#include "emulator/emulator.h"

uint32_t tga_palette[16] = {
        0x000000, // Black
        0x0000AA, // Dark Blue
        0x00AA00, // Dark Green
        0x00AAAA, // Teal
        0xAA0000, // Dark Red
        0xAA00AA, // Purple
        0xAA5500, // Brown
        0xAAAAAA, // Light Gray
        0x555555, // Dark Gray
        0x5555FF, // Blue
        0x55FF55, // Green
        0x55FFFF, // Aqua
        0xFF5555, // Red
        0xFF55FF, // Magenta
        0xFFFF55, // Yellow
        0xFFFFFF // White
};


inline void tga_portout(uint16_t portnum, uint16_t value) {
// http://archives.oldskool.org/pub/tvdog/tandy1000/faxback/02506.pdf
// https://ia803208.us.archive.org/15/items/Tandy_1000_Computer_Service_Manual_1985_Tandy/Tandy_1000_Computer_Service_Manual_1985_Tandy.pdf
    static uint8_t tga_register;

//    printf("TANDY %x %x \n", portnum, value);

    switch (portnum) {
        case 0x3DA: // address
            tga_register = value & 0x1F;
            break;
        case 0x3DE: // data
            switch (tga_register & 0x1F) {
                case 0x01: /* Palette Mask Register */
                case 0x02: /* Border Color */
                    return;
                case 0x03: /* Mode Control */
                    if (value == 0x10) {
                        videomode = 0x0A; //
                    }

                    if (value == 8) {
                        videomode = 0x8; // 640x200x4
                    }

                    return;
            }
            // Palette Registers 0x10-0x1F
            uint8_t palette = tga_register & 0xF;


            const uint8_t r = ((value >> 2 & 1) << 1) + (value >> 3 & 1);
            const uint8_t g = ((value >> 1 & 1) << 1) + (value >> 3 & 1);
            const uint8_t b = ((value >> 0 & 1) << 1) + (value >> 3 & 1);

            tga_palette[palette] = rgb(r * 85, g * 85, b * 85);
            break;
    }
}