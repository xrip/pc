#pragma once


#ifndef TFT_RST_PIN
#define TFT_RST_PIN 8
#endif


#ifndef TFT_CS_PIN
#define TFT_CS_PIN 6
#endif


#ifndef TFT_LED_PIN
#define TFT_LED_PIN 9
#endif


#ifndef TFT_CLK_PIN
#define TFT_CLK_PIN 13
#endif 

#ifndef TFT_DATA_PIN
#define TFT_DATA_PIN 12
#endif

#ifndef TFT_DC_PIN
#define TFT_DC_PIN 10
#endif

#define TEXTMODE_COLS 40
#define TEXTMODE_ROWS 25

#define rgb(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
static const uint16_t textmode_palette[16] = {
    //R, G, B
    rgb(0x00,0x00, 0x00), //black
    rgb(0x00,0x00, 0xC4), //blue
    rgb(0x00,0xC4, 0x00), //green
    rgb(0x00,0xC4, 0xC4), //cyan
    rgb(0xC4,0x00, 0x00), //red
    rgb(0xC4,0x00, 0xC4), //magenta
    rgb(0xC4,0x7E, 0x00), //brown
    rgb(0xC4,0xC4, 0xC4), //light gray
    rgb(0x4E,0x4E, 0x4E), //dark gray
    rgb(0x4E,0x4E, 0xDC), //light blue
    rgb(0x4E,0xDC, 0x4E), //light green
    rgb(0x4E,0xF3, 0xF3), //light cyan
    rgb(0xDC,0x4E, 0x4E), //light red
    rgb(0xF3,0x4E, 0xF3), //light magenta
    rgb(0xF3,0xF3, 0x4E), //yellow
    rgb(0xFF,0xFF, 0xFF), //white
};

inline static void graphics_set_bgcolor(uint32_t color888) {
    // dummy
}
inline static void graphics_set_flashmode(bool flash_line, bool flash_frame) {
    // dummy
}
void refresh_lcd();
