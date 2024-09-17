#include "emulator/emulator.h"

uint8_t vga_register;
static uint8_t color_index = 0, read_color_index = 0;

uint32_t vga_palette[256] = {
        rgb (0, 0, 0),
        rgb (0, 0, 169),
        rgb (0, 169, 0),
        rgb (0, 169, 169),
        rgb (169, 0, 0),
        rgb (169, 0, 169),
        rgb (169, 169, 0),
        rgb (169, 169, 169),
        rgb (0, 0, 84),
        rgb (0, 0, 255),
        rgb (0, 169, 84),
        rgb (0, 169, 255),
        rgb (169, 0, 84),
        rgb (169, 0, 255),
        rgb (169, 169, 84),
        rgb (169, 169, 255),
        rgb (0, 84, 0),
        rgb (0, 84, 169),
        rgb (0, 255, 0),
        rgb (0, 255, 169),
        rgb (169, 84, 0),
        rgb (169, 84, 169),
        rgb (169, 255, 0),
        rgb (169, 255, 169),
        rgb (0, 84, 84),
        rgb (0, 84, 255),
        rgb (0, 255, 84),
        rgb (0, 255, 255),
        rgb (169, 84, 84),
        rgb (169, 84, 255),
        rgb (169, 255, 84),
        rgb (169, 255, 255),
        rgb (84, 0, 0),
        rgb (84, 0, 169),
        rgb (84, 169, 0),
        rgb (84, 169, 169),
        rgb (255, 0, 0),
        rgb (255, 0, 169),
        rgb (255, 169, 0),
        rgb (255, 169, 169),
        rgb (84, 0, 84),
        rgb (84, 0, 255),
        rgb (84, 169, 84),
        rgb (84, 169, 255),
        rgb (255, 0, 84),
        rgb (255, 0, 255),
        rgb (255, 169, 84),
        rgb (255, 169, 255),
        rgb (84, 84, 0),
        rgb (84, 84, 169),
        rgb (84, 255, 0),
        rgb (84, 255, 169),
        rgb (255, 84, 0),
        rgb (255, 84, 169),
        rgb (255, 255, 0),
        rgb (255, 255, 169),
        rgb (84, 84, 84),
        rgb (84, 84, 255),
        rgb (84, 255, 84),
        rgb (84, 255, 255),
        rgb (255, 84, 84),
        rgb (255, 84, 255),
        rgb (255, 255, 84),
        rgb (255, 255, 255),
        rgb (255, 125, 125),
        rgb (255, 157, 125),
        rgb (255, 190, 125),
        rgb (255, 222, 125),
        rgb (255, 255, 125),
        rgb (222, 255, 125),
        rgb (190, 255, 125),
        rgb (157, 255, 125),
        rgb (125, 255, 125),
        rgb (125, 255, 157),
        rgb (125, 255, 190),
        rgb (125, 255, 222),
        rgb (125, 255, 255),
        rgb (125, 222, 255),
        rgb (125, 190, 255),
        rgb (125, 157, 255),
        rgb (182, 182, 255),
        rgb (198, 182, 255),
        rgb (218, 182, 255),
        rgb (234, 182, 255),
        rgb (255, 182, 255),
        rgb (255, 182, 234),
        rgb (255, 182, 218),
        rgb (255, 182, 198),
        rgb (255, 182, 182),
        rgb (255, 198, 182),
        rgb (255, 218, 182),
        rgb (255, 234, 182),
        rgb (255, 255, 182),
        rgb (234, 255, 182),
        rgb (218, 255, 182),
        rgb (198, 255, 182),
        rgb (182, 255, 182),
        rgb (182, 255, 198),
        rgb (182, 255, 218),
        rgb (182, 255, 234),
        rgb (182, 255, 255),
        rgb (182, 234, 255),
        rgb (182, 218, 255),
        rgb (182, 198, 255),
        rgb (0, 0, 113),
        rgb (28, 0, 113),
        rgb (56, 0, 113),
        rgb (84, 0, 113),
        rgb (113, 0, 113),
        rgb (113, 0, 84),
        rgb (113, 0, 56),
        rgb (113, 0, 28),
        rgb (113, 0, 0),
        rgb (113, 28, 0),
        rgb (113, 56, 0),
        rgb (113, 84, 0),
        rgb (113, 113, 0),
        rgb (84, 113, 0),
        rgb (56, 113, 0),
        rgb (28, 113, 0),
        rgb (0, 113, 0),
        rgb (0, 113, 28),
        rgb (0, 113, 56),
        rgb (0, 113, 84),
        rgb (0, 113, 113),
        rgb (0, 84, 113),
        rgb (0, 56, 113),
        rgb (0, 28, 113),
        rgb (56, 56, 113),
        rgb (68, 56, 113),
        rgb (84, 56, 113),
        rgb (97, 56, 113),
        rgb (113, 56, 113),
        rgb (113, 56, 97),
        rgb (113, 56, 84),
        rgb (113, 56, 68),
        rgb (113, 56, 56),
        rgb (113, 68, 56),
        rgb (113, 84, 56),
        rgb (113, 97, 56),
        rgb (113, 113, 56),
        rgb (97, 113, 56),
        rgb (84, 113, 56),
        rgb (68, 113, 56),
        rgb (56, 113, 56),
        rgb (56, 113, 68),
        rgb (56, 113, 84),
        rgb (56, 113, 97),
        rgb (56, 113, 113),
        rgb (56, 97, 113),
        rgb (56, 84, 113),
        rgb (56, 68, 113),
        rgb (80, 80, 113),
        rgb (89, 80, 113),
        rgb (97, 80, 113),
        rgb (105, 80, 113),
        rgb (113, 80, 113),
        rgb (113, 80, 105),
        rgb (113, 80, 97),
        rgb (113, 80, 89),
        rgb (113, 80, 80),
        rgb (113, 89, 80),
        rgb (113, 97, 80),
        rgb (113, 105, 80),
        rgb (113, 113, 80),
        rgb (105, 113, 80),
        rgb (97, 113, 80),
        rgb (89, 113, 80),
        rgb (80, 113, 80),
        rgb (80, 113, 89),
        rgb (80, 113, 97),
        rgb (80, 113, 105),
        rgb (80, 113, 113),
        rgb (80, 105, 113),
        rgb (80, 97, 113),
        rgb (80, 89, 113),
        rgb (0, 0, 64),
        rgb (16, 0, 64),
        rgb (32, 0, 64),
        rgb (48, 0, 64),
        rgb (64, 0, 64),
        rgb (64, 0, 48),
        rgb (64, 0, 32),
        rgb (64, 0, 16),
        rgb (64, 0, 0),
        rgb (64, 16, 0),
        rgb (64, 32, 0),
        rgb (64, 48, 0),
        rgb (64, 64, 0),
        rgb (48, 64, 0),
        rgb (32, 64, 0),
        rgb (16, 64, 0),
        rgb (0, 64, 0),
        rgb (0, 64, 16),
        rgb (0, 64, 32),
        rgb (0, 64, 48),
        rgb (0, 64, 64),
        rgb (0, 48, 64),
        rgb (0, 32, 64),
        rgb (0, 16, 64),
        rgb (32, 32, 64),
        rgb (40, 32, 64),
        rgb (48, 32, 64),
        rgb (56, 32, 64),
        rgb (64, 32, 64),
        rgb (64, 32, 56),
        rgb (64, 32, 48),
        rgb (64, 32, 40),
        rgb (64, 32, 32),
        rgb (64, 40, 32),
        rgb (64, 48, 32),
        rgb (64, 56, 32),
        rgb (64, 64, 32),
        rgb (56, 64, 32),
        rgb (48, 64, 32),
        rgb (40, 64, 32),
        rgb (32, 64, 32),
        rgb (32, 64, 40),
        rgb (32, 64, 48),
        rgb (32, 64, 56),
        rgb (32, 64, 64),
        rgb (32, 56, 64),
        rgb (32, 48, 64),
        rgb (32, 40, 64),
        rgb (44, 44, 64),
        rgb (48, 44, 64),
        rgb (52, 44, 64),
        rgb (60, 44, 64),
        rgb (64, 44, 64),
        rgb (64, 44, 60),
        rgb (64, 44, 52),
        rgb (64, 44, 48),
        rgb (64, 44, 44),
        rgb (64, 48, 44),
        rgb (64, 52, 44),
        rgb (64, 60, 44),
        rgb (64, 64, 44),
        rgb (60, 64, 44),
        rgb (52, 64, 44),
        rgb (48, 64, 44),
        rgb (44, 64, 44),
        rgb (44, 64, 48),
        rgb (44, 64, 52),
        rgb (44, 64, 60),
        rgb (44, 64, 64),
        rgb (44, 60, 64),
        rgb (44, 52, 64),
        rgb (44, 48, 64),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
        rgb (0, 0, 0),
};

void vga_portout(uint16_t portnum, uint16_t value) {
    if (portnum != 0x3c8 && portnum != 0x3c9)
        printf("vga_portout %x %x\n", portnum, value);
    switch (portnum) {
        case 0x3C0: {
            static uint8_t data_mode = 0; // 0 -- address, 1 -- data

            if (data_mode) {
                // Palette registers
                if (vga_register <= 0x0f) {
                    const uint8_t r = (value & 0b001 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                    const uint8_t g = (value & 0b010 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                    const uint8_t b = (value & 0b100 ? 2 : 0) + (value & 0b111000 ? 1 : 0);

                    vga_palette[vga_register] = rgb(b * 85, g * 85, r * 85);
                } else {
                    // vga[vga_register_index] = value;
                }
            } else {
                vga_register = value & 0b1111;
            }

            data_mode ^= 1;
            break;
        }
        case 0x3C7:
            read_color_index = value & 0xff;
            break;
        case 0x3C8: //color index register (write operations)
            color_index = value & 0xff;
            break;
        case 0x3C9: { //RGB data register
            static uint8_t RGB[3] = { 0, 0, 0 };
            static uint8_t rgb_index = 0;

            RGB[rgb_index++] = value << 2;
            if (rgb_index == 3) {
                vga_palette[color_index++] = rgb(RGB[0], RGB[1], RGB[2]);
                rgb_index = 0;
            }

        }
    }
}

uint16_t vga_portin(uint16_t portnum) {
    printf("vga_portin %x\n", portnum);

    switch (portnum) {
        case 0x3C8:
            return read_color_index;
        case 0x3C9: {
            static uint8_t rgb_index = 0;
            switch (rgb_index++) {
                case 0: return ((vga_palette[read_color_index] >> 18))  & 63;
                case 1: return ((vga_palette[read_color_index] >> 10))  & 63;
                case 2: rgb_index = 0; return ((vga_palette[read_color_index++] >> 2)) & 63;
            }
        }
    }
}