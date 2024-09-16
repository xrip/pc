#include <cstdio>
#include <windows.h>
#include "MiniFB.h"
#include "emulator/font8x16.h"
#include "emulator/emulator.h"
#include "emulator/font8x8.h"

static uint8_t SCREEN[400][640];

int cursor_blink_state = 0;

HANDLE hComm;
DWORD bytesWritten;
DCB dcb;

#define AUDIO_FREQ 44100
#define AUDIO_BUFFER_LENGTH ((AUDIO_FREQ /60 +1) * 2)

#define rgb(r, g, b) ((r<<16) | (g << 8 ) | b )

int16_t audiobuffer[AUDIO_BUFFER_LENGTH] = { 0 };

DWORD WINAPI SoundThread(LPVOID lpParam) {
    WAVEHDR waveHeaders[4];

    WAVEFORMATEX format = { 0 };
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = AUDIO_FREQ;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    HANDLE waveEvent = CreateEvent(NULL, 1, 0, NULL);

    HWAVEOUT hWaveOut;
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, (DWORD_PTR) waveEvent, 0, CALLBACK_EVENT);

    for (size_t i = 0; i < 4; i++) {
        int16_t audio_buffers[4][AUDIO_BUFFER_LENGTH * 2];
        waveHeaders[i] = {
                .lpData = (char *) audio_buffers[i],
                .dwBufferLength = AUDIO_BUFFER_LENGTH * 2,
        };
        waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        waveHeaders[i].dwFlags |= WHDR_DONE;
    }
    WAVEHDR *currentHeader = waveHeaders;


    while (true) {
        if (WaitForSingleObject(waveEvent, INFINITE)) {
            fprintf(stderr, "Failed to wait for event.\n");
            return 1;
        }

        if (!ResetEvent(waveEvent)) {
            fprintf(stderr, "Failed to reset event.\n");
            return 1;
        }

// Wait until audio finishes playing
        while (currentHeader->dwFlags & WHDR_DONE) {
//            PSG_calc_stereo(&psg, audiobuffer, AUDIO_BUFFER_LENGTH);
            memcpy(currentHeader->lpData, audiobuffer, AUDIO_BUFFER_LENGTH * 2);
            waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));

            currentHeader++;
            if (currentHeader == waveHeaders + 4) { currentHeader = waveHeaders; }
        }
    }
    return 0;
}

DWORD WINAPI TicksThread(LPVOID lpParam) {
    LARGE_INTEGER start, current;
    QueryPerformanceCounter(&start);  // Get the starting time

    uint8_t localVRAM[VIDEORAM_SIZE]= { 0 };
    double elapsed_system_timer = 0;
    double elapsed_blink_tics = 0;
    double elapsed_frame_tics = 0;

    while (true) {
        QueryPerformanceCounter(&current);  // Get the current time

        // Calculate elapsed time in ticks since the start
        auto elapsedTime = (double) (current.QuadPart - start.QuadPart);

        if (elapsedTime - elapsed_system_timer >= timer_period) {
            doirq(0);
            elapsed_system_timer = elapsedTime;  // Reset the tick counter for 1Hz
        }

        if (elapsedTime - elapsed_blink_tics >= 500'000'0) {
            cursor_blink_state ^= 1;
            elapsed_blink_tics = elapsedTime;  // Reset the tick counter for 2Hz
        }

        if (elapsedTime - elapsed_frame_tics >= 16'666){
            if (1) {
                port3DA = 1;
                static uint8_t v = 0;
                if (v != videomode) {
                    printf("videomode %x\n", videomode);
                    v = videomode;
                    vram_offset = 0;
                    if (videomode == 0x13) {
                        uint32_t palettevga[256];
                        palettevga[0] = rgb (0, 0, 0);
                        palettevga[1] = rgb (0, 0, 169);
                        palettevga[2] = rgb (0, 169, 0);
                        palettevga[3] = rgb (0, 169, 169);
                        palettevga[4] = rgb (169, 0, 0);
                        palettevga[5] = rgb (169, 0, 169);
                        palettevga[6] = rgb (169, 169, 0);
                        palettevga[7] = rgb (169, 169, 169);
                        palettevga[8] = rgb (0, 0, 84);
                        palettevga[9] = rgb (0, 0, 255);
                        palettevga[10] = rgb (0, 169, 84);
                        palettevga[11] = rgb (0, 169, 255);
                        palettevga[12] = rgb (169, 0, 84);
                        palettevga[13] = rgb (169, 0, 255);
                        palettevga[14] = rgb (169, 169, 84);
                        palettevga[15] = rgb (169, 169, 255);
                        palettevga[16] = rgb (0, 84, 0);
                        palettevga[17] = rgb (0, 84, 169);
                        palettevga[18] = rgb (0, 255, 0);
                        palettevga[19] = rgb (0, 255, 169);
                        palettevga[20] = rgb (169, 84, 0);
                        palettevga[21] = rgb (169, 84, 169);
                        palettevga[22] = rgb (169, 255, 0);
                        palettevga[23] = rgb (169, 255, 169);
                        palettevga[24] = rgb (0, 84, 84);
                        palettevga[25] = rgb (0, 84, 255);
                        palettevga[26] = rgb (0, 255, 84);
                        palettevga[27] = rgb (0, 255, 255);
                        palettevga[28] = rgb (169, 84, 84);
                        palettevga[29] = rgb (169, 84, 255);
                        palettevga[30] = rgb (169, 255, 84);
                        palettevga[31] = rgb (169, 255, 255);
                        palettevga[32] = rgb (84, 0, 0);
                        palettevga[33] = rgb (84, 0, 169);
                        palettevga[34] = rgb (84, 169, 0);
                        palettevga[35] = rgb (84, 169, 169);
                        palettevga[36] = rgb (255, 0, 0);
                        palettevga[37] = rgb (255, 0, 169);
                        palettevga[38] = rgb (255, 169, 0);
                        palettevga[39] = rgb (255, 169, 169);
                        palettevga[40] = rgb (84, 0, 84);
                        palettevga[41] = rgb (84, 0, 255);
                        palettevga[42] = rgb (84, 169, 84);
                        palettevga[43] = rgb (84, 169, 255);
                        palettevga[44] = rgb (255, 0, 84);
                        palettevga[45] = rgb (255, 0, 255);
                        palettevga[46] = rgb (255, 169, 84);
                        palettevga[47] = rgb (255, 169, 255);
                        palettevga[48] = rgb (84, 84, 0);
                        palettevga[49] = rgb (84, 84, 169);
                        palettevga[50] = rgb (84, 255, 0);
                        palettevga[51] = rgb (84, 255, 169);
                        palettevga[52] = rgb (255, 84, 0);
                        palettevga[53] = rgb (255, 84, 169);
                        palettevga[54] = rgb (255, 255, 0);
                        palettevga[55] = rgb (255, 255, 169);
                        palettevga[56] = rgb (84, 84, 84);
                        palettevga[57] = rgb (84, 84, 255);
                        palettevga[58] = rgb (84, 255, 84);
                        palettevga[59] = rgb (84, 255, 255);
                        palettevga[60] = rgb (255, 84, 84);
                        palettevga[61] = rgb (255, 84, 255);
                        palettevga[62] = rgb (255, 255, 84);
                        palettevga[63] = rgb (255, 255, 255);
                        palettevga[64] = rgb (255, 125, 125);
                        palettevga[65] = rgb (255, 157, 125);
                        palettevga[66] = rgb (255, 190, 125);
                        palettevga[67] = rgb (255, 222, 125);
                        palettevga[68] = rgb (255, 255, 125);
                        palettevga[69] = rgb (222, 255, 125);
                        palettevga[70] = rgb (190, 255, 125);
                        palettevga[71] = rgb (157, 255, 125);
                        palettevga[72] = rgb (125, 255, 125);
                        palettevga[73] = rgb (125, 255, 157);
                        palettevga[74] = rgb (125, 255, 190);
                        palettevga[75] = rgb (125, 255, 222);
                        palettevga[76] = rgb (125, 255, 255);
                        palettevga[77] = rgb (125, 222, 255);
                        palettevga[78] = rgb (125, 190, 255);
                        palettevga[79] = rgb (125, 157, 255);
                        palettevga[80] = rgb (182, 182, 255);
                        palettevga[81] = rgb (198, 182, 255);
                        palettevga[82] = rgb (218, 182, 255);
                        palettevga[83] = rgb (234, 182, 255);
                        palettevga[84] = rgb (255, 182, 255);
                        palettevga[85] = rgb (255, 182, 234);
                        palettevga[86] = rgb (255, 182, 218);
                        palettevga[87] = rgb (255, 182, 198);
                        palettevga[88] = rgb (255, 182, 182);
                        palettevga[89] = rgb (255, 198, 182);
                        palettevga[90] = rgb (255, 218, 182);
                        palettevga[91] = rgb (255, 234, 182);
                        palettevga[92] = rgb (255, 255, 182);
                        palettevga[93] = rgb (234, 255, 182);
                        palettevga[94] = rgb (218, 255, 182);
                        palettevga[95] = rgb (198, 255, 182);
                        palettevga[96] = rgb (182, 255, 182);
                        palettevga[97] = rgb (182, 255, 198);
                        palettevga[98] = rgb (182, 255, 218);
                        palettevga[99] = rgb (182, 255, 234);
                        palettevga[100] = rgb (182, 255, 255);
                        palettevga[101] = rgb (182, 234, 255);
                        palettevga[102] = rgb (182, 218, 255);
                        palettevga[103] = rgb (182, 198, 255);
                        palettevga[104] = rgb (0, 0, 113);
                        palettevga[105] = rgb (28, 0, 113);
                        palettevga[106] = rgb (56, 0, 113);
                        palettevga[107] = rgb (84, 0, 113);
                        palettevga[108] = rgb (113, 0, 113);
                        palettevga[109] = rgb (113, 0, 84);
                        palettevga[110] = rgb (113, 0, 56);
                        palettevga[111] = rgb (113, 0, 28);
                        palettevga[112] = rgb (113, 0, 0);
                        palettevga[113] = rgb (113, 28, 0);
                        palettevga[114] = rgb (113, 56, 0);
                        palettevga[115] = rgb (113, 84, 0);
                        palettevga[116] = rgb (113, 113, 0);
                        palettevga[117] = rgb (84, 113, 0);
                        palettevga[118] = rgb (56, 113, 0);
                        palettevga[119] = rgb (28, 113, 0);
                        palettevga[120] = rgb (0, 113, 0);
                        palettevga[121] = rgb (0, 113, 28);
                        palettevga[122] = rgb (0, 113, 56);
                        palettevga[123] = rgb (0, 113, 84);
                        palettevga[124] = rgb (0, 113, 113);
                        palettevga[125] = rgb (0, 84, 113);
                        palettevga[126] = rgb (0, 56, 113);
                        palettevga[127] = rgb (0, 28, 113);
                        palettevga[128] = rgb (56, 56, 113);
                        palettevga[129] = rgb (68, 56, 113);
                        palettevga[130] = rgb (84, 56, 113);
                        palettevga[131] = rgb (97, 56, 113);
                        palettevga[132] = rgb (113, 56, 113);
                        palettevga[133] = rgb (113, 56, 97);
                        palettevga[134] = rgb (113, 56, 84);
                        palettevga[135] = rgb (113, 56, 68);
                        palettevga[136] = rgb (113, 56, 56);
                        palettevga[137] = rgb (113, 68, 56);
                        palettevga[138] = rgb (113, 84, 56);
                        palettevga[139] = rgb (113, 97, 56);
                        palettevga[140] = rgb (113, 113, 56);
                        palettevga[141] = rgb (97, 113, 56);
                        palettevga[142] = rgb (84, 113, 56);
                        palettevga[143] = rgb (68, 113, 56);
                        palettevga[144] = rgb (56, 113, 56);
                        palettevga[145] = rgb (56, 113, 68);
                        palettevga[146] = rgb (56, 113, 84);
                        palettevga[147] = rgb (56, 113, 97);
                        palettevga[148] = rgb (56, 113, 113);
                        palettevga[149] = rgb (56, 97, 113);
                        palettevga[150] = rgb (56, 84, 113);
                        palettevga[151] = rgb (56, 68, 113);
                        palettevga[152] = rgb (80, 80, 113);
                        palettevga[153] = rgb (89, 80, 113);
                        palettevga[154] = rgb (97, 80, 113);
                        palettevga[155] = rgb (105, 80, 113);
                        palettevga[156] = rgb (113, 80, 113);
                        palettevga[157] = rgb (113, 80, 105);
                        palettevga[158] = rgb (113, 80, 97);
                        palettevga[159] = rgb (113, 80, 89);
                        palettevga[160] = rgb (113, 80, 80);
                        palettevga[161] = rgb (113, 89, 80);
                        palettevga[162] = rgb (113, 97, 80);
                        palettevga[163] = rgb (113, 105, 80);
                        palettevga[164] = rgb (113, 113, 80);
                        palettevga[165] = rgb (105, 113, 80);
                        palettevga[166] = rgb (97, 113, 80);
                        palettevga[167] = rgb (89, 113, 80);
                        palettevga[168] = rgb (80, 113, 80);
                        palettevga[169] = rgb (80, 113, 89);
                        palettevga[170] = rgb (80, 113, 97);
                        palettevga[171] = rgb (80, 113, 105);
                        palettevga[172] = rgb (80, 113, 113);
                        palettevga[173] = rgb (80, 105, 113);
                        palettevga[174] = rgb (80, 97, 113);
                        palettevga[175] = rgb (80, 89, 113);
                        palettevga[176] = rgb (0, 0, 64);
                        palettevga[177] = rgb (16, 0, 64);
                        palettevga[178] = rgb (32, 0, 64);
                        palettevga[179] = rgb (48, 0, 64);
                        palettevga[180] = rgb (64, 0, 64);
                        palettevga[181] = rgb (64, 0, 48);
                        palettevga[182] = rgb (64, 0, 32);
                        palettevga[183] = rgb (64, 0, 16);
                        palettevga[184] = rgb (64, 0, 0);
                        palettevga[185] = rgb (64, 16, 0);
                        palettevga[186] = rgb (64, 32, 0);
                        palettevga[187] = rgb (64, 48, 0);
                        palettevga[188] = rgb (64, 64, 0);
                        palettevga[189] = rgb (48, 64, 0);
                        palettevga[190] = rgb (32, 64, 0);
                        palettevga[191] = rgb (16, 64, 0);
                        palettevga[192] = rgb (0, 64, 0);
                        palettevga[193] = rgb (0, 64, 16);
                        palettevga[194] = rgb (0, 64, 32);
                        palettevga[195] = rgb (0, 64, 48);
                        palettevga[196] = rgb (0, 64, 64);
                        palettevga[197] = rgb (0, 48, 64);
                        palettevga[198] = rgb (0, 32, 64);
                        palettevga[199] = rgb (0, 16, 64);
                        palettevga[200] = rgb (32, 32, 64);
                        palettevga[201] = rgb (40, 32, 64);
                        palettevga[202] = rgb (48, 32, 64);
                        palettevga[203] = rgb (56, 32, 64);
                        palettevga[204] = rgb (64, 32, 64);
                        palettevga[205] = rgb (64, 32, 56);
                        palettevga[206] = rgb (64, 32, 48);
                        palettevga[207] = rgb (64, 32, 40);
                        palettevga[208] = rgb (64, 32, 32);
                        palettevga[209] = rgb (64, 40, 32);
                        palettevga[210] = rgb (64, 48, 32);
                        palettevga[211] = rgb (64, 56, 32);
                        palettevga[212] = rgb (64, 64, 32);
                        palettevga[213] = rgb (56, 64, 32);
                        palettevga[214] = rgb (48, 64, 32);
                        palettevga[215] = rgb (40, 64, 32);
                        palettevga[216] = rgb (32, 64, 32);
                        palettevga[217] = rgb (32, 64, 40);
                        palettevga[218] = rgb (32, 64, 48);
                        palettevga[219] = rgb (32, 64, 56);
                        palettevga[220] = rgb (32, 64, 64);
                        palettevga[221] = rgb (32, 56, 64);
                        palettevga[222] = rgb (32, 48, 64);
                        palettevga[223] = rgb (32, 40, 64);
                        palettevga[224] = rgb (44, 44, 64);
                        palettevga[225] = rgb (48, 44, 64);
                        palettevga[226] = rgb (52, 44, 64);
                        palettevga[227] = rgb (60, 44, 64);
                        palettevga[228] = rgb (64, 44, 64);
                        palettevga[229] = rgb (64, 44, 60);
                        palettevga[230] = rgb (64, 44, 52);
                        palettevga[231] = rgb (64, 44, 48);
                        palettevga[232] = rgb (64, 44, 44);
                        palettevga[233] = rgb (64, 48, 44);
                        palettevga[234] = rgb (64, 52, 44);
                        palettevga[235] = rgb (64, 60, 44);
                        palettevga[236] = rgb (64, 64, 44);
                        palettevga[237] = rgb (60, 64, 44);
                        palettevga[238] = rgb (52, 64, 44);
                        palettevga[239] = rgb (48, 64, 44);
                        palettevga[240] = rgb (44, 64, 44);
                        palettevga[241] = rgb (44, 64, 48);
                        palettevga[242] = rgb (44, 64, 52);
                        palettevga[243] = rgb (44, 64, 60);
                        palettevga[244] = rgb (44, 64, 64);
                        palettevga[245] = rgb (44, 60, 64);
                        palettevga[246] = rgb (44, 52, 64);
                        palettevga[247] = rgb (44, 48, 64);
                        palettevga[248] = rgb (0, 0, 0);
                        palettevga[249] = rgb (0, 0, 0);
                        palettevga[250] = rgb (0, 0, 0);
                        palettevga[251] = rgb (0, 0, 0);
                        palettevga[252] = rgb (0, 0, 0);
                        palettevga[253] = rgb (0, 0, 0);
                        palettevga[254] = rgb (0, 0, 0);
                        palettevga[255] = rgb (0, 0, 0);
                        mfb_set_pallete(palettevga, 0, 255);

                    }
                }


                memcpy(localVRAM, VIDEORAM+ 0x18000 + (vram_offset << 1), VIDEORAM_SIZE);
                uint8_t *vidramptr = localVRAM;
                const uint8_t cols = videomode <= 1 ? 40 : 80;
                for (uint16_t y = 0; y < 400; y++)
                    switch (videomode) {
                        case 0x00:
                        case 0x01: {
                            uint16_t y_div_16 = y / 16;       // Precompute y / 16
                            uint8_t y_mod_8 = (y / 2) % 8;    // Precompute y % 8 for font lookup

                            for (uint8_t column = 0; column < cols; column++) {
                                // Access vidram and font data once per character
                                uint8_t charcode = vidramptr[y_div_16 * (cols * 2) + column * 2];  // Character code
                                uint8_t glyph_row = font_8x8[charcode * 8 + y_mod_8];              // Glyph row from font
                                uint8_t color = vidramptr[y_div_16 * (cols * 2) + column * 2 + 1];  // Color attribute

                                // Calculate screen position
                                uint8_t *screenptr = (uint8_t *) &SCREEN[0][0] + y * 640 + (8 * column) * 2;

                                // Cursor blinking check
                                uint8_t cursor_active = (cursor_blink_state &&
                                                         (y_div_16 == CURSOR_Y && column == CURSOR_X &&
                                                          y_mod_8 >= cursor_start &&
                                                          y_mod_8 <= cursor_end));

                                // Unrolled bit loop: Write 8 pixels with scaling (2x horizontally)
                                for (uint8_t bit = 0; bit < 8; bit++) {
                                    uint8_t pixel_color;
                                    if (cursor_active) {
                                        pixel_color = color & 0x0F;  // Cursor foreground color
                                    } else if ((color >> 7) & 1 && cursor_blink_state) {
                                        pixel_color = (color >> 4) & 0x07;  // Blinking background color
                                    } else {
                                        pixel_color = (glyph_row >> bit) & 1 ? (color & 0x0f) : (color
                                                >> 4);  // Foreground or background color
                                    }

                                    // Write the pixel twice (horizontal scaling)
                                    *screenptr++ = pixel_color;
                                    *screenptr++ = pixel_color;
                                }
                            }


                            break;
                        }
                        case 0x02:
                        case 0x03: {
                            uint16_t y_div_16 = y / 16;       // Precompute y / 16
                            uint8_t y_mod_16 = y % 16;    // Precompute y % 8 for font lookup

                            for (uint8_t column = 0; column < cols; column++) {
                                // Access vidram and font data once per character
                                const uint16_t column_offset = y_div_16 * (cols * 2) + column * 2;
                                uint8_t charcode = vidramptr[column_offset];  // Character code
                                uint8_t color = vidramptr[column_offset + 1]; // Color attribute
                                uint8_t glyph_row = font_8x16[charcode * 16 + y_mod_16];              // Glyph row from font

                                // Calculate screen position
                                uint8_t *screenptr = (uint8_t *) &SCREEN[0][0] + y * 640 + (8 * column);

                                // Cursor blinking check
                                uint8_t cursor_active = cursor_blink_state && y_div_16 == CURSOR_Y && column == CURSOR_X &&
                                                        (cursor_start > cursor_end ? !(y_mod_16 >= cursor_end << 1 &&
                                                                                       y_mod_16 <= cursor_start << 1) :
                                                         y_mod_16 >= cursor_start << 1 && y_mod_16 <= cursor_end << 1);

                                // Unrolled bit loop: Write 8 pixels with scaling (2x horizontally)
                                for (uint8_t bit = 0; bit < 8; bit++) {
                                    uint8_t pixel_color;
                                    if (cursor_active) {
                                        pixel_color = color & 0x0F;  // Cursor foreground color
                                    } else if (cga_blinking && (color >> 7) & 1 && cursor_blink_state) {
                                        pixel_color = (color >> 4) & 0x07;  // Blinking background color
                                    } else {
                                        pixel_color = (glyph_row >> bit) & 1 ? (color & 0x0f) : (color
                                                >> 4);  // Foreground or background color
                                    }

                                    // Write the pixel twice (horizontal scaling)
                                    *screenptr++ = pixel_color;
                                }
                            }
                            break;
                        }
                        case 0x04:
                        case 0x05: {
                            uint8_t *pixels = (uint8_t *) &SCREEN[y][0];
                            uint8_t *cga_row = &vidramptr[((((y / 2) >> 1) * 80) +
                                                          (((y / 2) & 1) * 8192))];  // Precompute CGA row pointer
                            uint8_t *current_cga_palette = (uint8_t *) cga_gfxpal[cga_colorset][cga_intensity];

                            for (int x = 0; x < 320; x += 4) {
                                uint8_t cga_byte = *cga_row++;  // Get the byte containing 4 pixels

                                // Extract all four 2-bit pixels from the CGA byte
                                // and write each pixel twice for horizontal scaling
                                *pixels++ = *pixels++ = (cga_byte >> 6) & 3 ? current_cga_palette[(cga_byte >> 6) & 3]
                                                                            : cga_foreground_color;
                                *pixels++ = *pixels++ = (cga_byte >> 4) & 3 ? current_cga_palette[(cga_byte >> 4) & 3]
                                                                            : cga_foreground_color;
                                *pixels++ = *pixels++ = (cga_byte >> 2) & 3 ? current_cga_palette[(cga_byte >> 2) & 3]
                                                                            : cga_foreground_color;
                                *pixels++ = *pixels++ = (cga_byte >> 0) & 3 ? current_cga_palette[(cga_byte >> 0) & 3]
                                                                            : cga_foreground_color;
                            }
                            break;
                        }
                        case 0x06: {
                            uint8_t *pixels = (uint8_t *) &SCREEN[y][0];
                            uint8_t *cga_row = &vidramptr[(((y / 2) >> 1) * 80) +
                                                          (((y / 2) & 1) * 8192)];  // Precompute row start

                            for (int x = 0; x < 640; x += 8) {
                                uint8_t cga_byte = *cga_row++;  // Fetch 8 pixels from CGA memory

                                *pixels++ = ((cga_byte >> 7) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 6) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 5) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 4) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 3) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 2) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 1) & 1) * cga_foreground_color;
                                *pixels++ = ((cga_byte >> 0) & 1) * cga_foreground_color;
                            }

                            break;
                        }
                        case 0x8:
                        case 0x74: /* 160x200x16    */
                        case 0x76: /* cga composite / tandy */ {
                            uint8_t color_offset = 16;
                            switch (videomode) {
                                case 0x08: color_offset = 64; break;
                                case 0x74: color_offset = 16; break;
                                case 0x76: color_offset += (cga_intensity + cga_colorset) * 16; break;

                            }

                            uint8_t *pixels = (uint8_t *) &SCREEN[y][0];
                            uint8_t *cga_row = &vidramptr[(((y / 2) >> 1) * 80) +
                                                          (((y / 2) & 1) * 8192)];  // Precompute row start

                            for (int x = 0; x < 640; x += 8) {
                                uint8_t cga_byte = *cga_row++;  // Fetch 8 pixels from CGA memory

                                *pixels++ = *pixels++ = *pixels++ = *pixels++ = color_offset + ((cga_byte >> 4) & 15);
                                *pixels++ = *pixels++ = *pixels++ = *pixels++ = color_offset + (cga_byte & 15);
                            }

                            break;
                        }
                        case 0x09: /* tandy 320x200 16 color */ {
                            uint8_t *pixels = (uint8_t *) &SCREEN[y][0];
                            uint8_t *tga_row = vidramptr + (((y / 2) & 3) * 8192) + ((y / 8) * 160);
                            for (int x = 320; x--;) {
                                uint8_t tga_byte = *tga_row++;
                                *pixels++ = (tga_byte >> 4) & 15;
                                *pixels++ = (tga_byte >> 4) & 15;
                                *pixels++ = tga_byte & 15;
                                *pixels++ = tga_byte & 15;
                            }
                            break;
                        }
                        case 0x0A: /* tandy 640x200 4 color */ {
                            // (cga_byte >> 0) & 3
                            printf("HEEELP!!\n");
                            uint8_t *pixels = (uint8_t *) &SCREEN[y][0];
                            uint8_t *tga_row = vidramptr + (((y / 2) & 3) * 8192) + ((y / 8) * 160);
                            for (int x = 640; x--;) {
                                uint8_t tga_byte = *tga_row++;
                                *pixels++ = (tga_byte >> 6) & 3;
                                *pixels++ = (tga_byte >> 4) & 3;
                                *pixels++ = (tga_byte >> 2) & 3;
                                *pixels++ = (tga_byte >> 0) & 3;
                            }
                            break;
                        }
                        case 0x13 : {
                            uint8_t *pixels = (uint8_t *) &SCREEN[y][0];
                            for (int x = 0; x < 640; x++) {
                                *pixels++ = VIDEORAM[(x >> 1) + (y >> 1) * 320];
                            }
                            break;
                        }

                        case 0x77: /* 160x100x16 textmode */ {
                            uint16_t y_div_16 = y / 4;       // Precompute y / 16
                            uint8_t y_mod_8 = (y / 2) % 2;    // Precompute y % 8 for font lookup

                            for (uint8_t column = 0; column < cols; column++) {
                                // Access vidram and font data once per character
                                uint8_t charcode = vidramptr[y_div_16 * (cols * 2) + column * 2];  // Character code
                                uint8_t glyph_row = font_8x16[charcode * 16 + y_mod_8];              // Glyph row from font
                                uint8_t color = vidramptr[y_div_16 * (cols * 2) + column * 2 + 1];  // Color attribute

                                // Calculate screen position
                                uint8_t *screenptr = (uint8_t *) &SCREEN[0][0] + y * 640 + (8 * column);

                                // Cursor blinking check
                                uint8_t cursor_active = (cursor_blink_state &&
                                                         (y_div_16 == CURSOR_Y && column == CURSOR_X && y_mod_8 >= 12 &&
                                                          y_mod_8 <= 13));

                                // Unrolled bit loop: Write 8 pixels with scaling (2x horizontally)
                                for (uint8_t bit = 0; bit < 8; bit++) {
                                    uint8_t pixel_color;
                                    if (cursor_active) {
                                        pixel_color = color & 0x0F;  // Cursor foreground color
                                    } else if (cga_blinking && (color >> 7) & 1 && cursor_blink_state) {
                                        pixel_color = (color >> 4) & 0x07;  // Blinking background color
                                    } else {
                                        pixel_color = (glyph_row >> bit) & 1 ? (color & 0x0f) : (color
                                                >> 4);  // Foreground or background color
                                    }

                                    *screenptr++ = pixel_color;
                                }
                            }
                            break;
                        }
                        default:
                            printf("Unsupported videomode %x\n", videomode);

                    }
            }
            port3DA = 0b1000;
            elapsed_frame_tics = elapsedTime;  // Reset the tick counter for 2Hz
        }

        //Sleep(1);
    }
}

uint8_t log_debug = 0;
extern "C" void HandleInput(WPARAM wParam, BOOL isKeyDown) {
    unsigned char scancode = 0;
    // Check if SCROLL LOCK is pressed
    if (wParam == VK_SCROLL && isKeyDown) {
        // Check if CTRL and ALT are pressed
        if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_MENU) & 0x8000)) {
            // Handle CTRL + ALT + SCROLL LOCK combination
//            MessageBox(nullptr, "CTRL + ALT + SCROLL LOCK detected!", "COMPOSITE MODE ON", MB_OK);
            static uint8_t old_vm;
            if (videomode == 4 || videomode == 6) {
                old_vm = videomode;
                videomode += 0x70;
            } else if (videomode >= 0x74) {
                videomode = old_vm;
            }
            vram_offset += 0x10;
//            log_debug = !log_debug;
        }
    }
    switch (wParam) {
        // Row 1
        case VK_ESCAPE:
            scancode = 0x01;
            break;
        case '1':
            scancode = 0x02;
            break;
        case '2':
            scancode = 0x03;
            break;
        case '3':
            scancode = 0x04;
            break;
        case '4':
            scancode = 0x05;
            break;
        case '5':
            scancode = 0x06;
            break;
        case '6':
            scancode = 0x07;
            break;
        case '7':
            scancode = 0x08;
            break;
        case '8':
            scancode = 0x09;
            break;
        case '9':
            scancode = 0x0A;
            break;
        case '0':
            scancode = 0x0B;
            break;
        case VK_OEM_MINUS:
            scancode = 0x0C;
            break;  // - key
        case VK_OEM_PLUS:
            scancode = 0x0D;
            break;   // = key
        case VK_BACK:
            scancode = 0x0E;
            break;       // Backspace

            // Row 2
        case VK_TAB:
            scancode = 0x0F;
            break;
        case 'Q':
            scancode = 0x10;
            break;
        case 'W':
            scancode = 0x11;
            break;
        case 'E':
            scancode = 0x12;
            break;
        case 'R':
            scancode = 0x13;
            break;
        case 'T':
            scancode = 0x14;
            break;
        case 'Y':
            scancode = 0x15;
            break;
        case 'U':
            scancode = 0x16;
            break;
        case 'I':
            scancode = 0x17;
            break;
        case 'O':
            scancode = 0x18;
            break;
        case 'P':
            scancode = 0x19;
            break;
        case VK_OEM_4:
            scancode = 0x1A;
            break;  // [ key
        case VK_OEM_6:
            scancode = 0x1B;
            break;  // ] key
        case VK_RETURN:
            scancode = 0x1C;
            break; // Enter

            // Row 3
        case VK_CONTROL:
            scancode = 0x1D;
            break;  // Left Control
        case 'A':
            scancode = 0x1E;
            break;
        case 'S':
            scancode = 0x1F;
            break;
        case 'D':
            scancode = 0x20;
            break;
        case 'F':
            scancode = 0x21;
            break;
        case 'G':
            scancode = 0x22;
            break;
        case 'H':
            scancode = 0x23;
            break;
        case 'J':
            scancode = 0x24;
            break;
        case 'K':
            scancode = 0x25;
            break;
        case 'L':
            scancode = 0x26;
            break;
        case VK_OEM_1:
            scancode = 0x27;
            break;  // ; key
        case VK_OEM_7:
            scancode = 0x28;
            break;  // ' key
        case VK_OEM_3:
            scancode = 0x29;
            break;  // ` key (backtick)

            // Row 4
        case VK_SHIFT:
            scancode = 0x2A;
            break;  // Left Shift
        case VK_OEM_5:
            scancode = 0x2B;
            break;  // \ key
        case 'Z':
            scancode = 0x2C;
            break;
        case 'X':
            scancode = 0x2D;
            break;
        case 'C':
            scancode = 0x2E;
            break;
        case 'V':
            scancode = 0x2F;
            break;
        case 'B':
            scancode = 0x30;
            break;
        case 'N':
            scancode = 0x31;
            break;
        case 'M':
            scancode = 0x32;
            break;
        case VK_OEM_COMMA:
            scancode = 0x33;
            break;  // , key
        case VK_OEM_PERIOD:
            scancode = 0x34;
            break; // . key
        case VK_OEM_2:
            scancode = 0x35;
            break;      // / key
        case VK_RSHIFT:
            scancode = 0x36;
            break;     // Right Shift

            // Row 5
        case VK_MULTIPLY:
            scancode = 0x37;
            break;  // Numpad *
        case VK_MENU:
            scancode = 0x38;
            break;      // Left Alt
        case VK_SPACE:
            scancode = 0x39;
            break;     // Spacebar
        case VK_CAPITAL:
            scancode = 0x3A;
            break;   // Caps Lock

            // F1-F10
        case VK_F1:
            scancode = 0x3B;
            break;
        case VK_F2:
            scancode = 0x3C;
            break;
        case VK_F3:
            scancode = 0x3D;
            break;
        case VK_F4:
            scancode = 0x3E;
            break;
        case VK_F5:
            scancode = 0x3F;
            break;
        case VK_F6:
            scancode = 0x40;
            break;
        case VK_F7:
            scancode = 0x41;
            break;
        case VK_F8:
            scancode = 0x42;
            break;
        case VK_F9:
            scancode = 0x43;
            break;
        case VK_F10:
            scancode = 0x44;
            break;

            // Numpad
        case VK_NUMLOCK:
            scancode = 0x45;
            break;
        case VK_SCROLL:
            scancode = 0x46;
            break;    // Scroll Lock
        case VK_NUMPAD7:
            scancode = 0x47;
            break;   // Numpad 7
        case VK_NUMPAD8:
            scancode = 0x48;
            break;   // Numpad 8
        case VK_NUMPAD9:
            scancode = 0x49;
            break;   // Numpad 9
        case VK_SUBTRACT:
            scancode = 0x4A;
            break;  // Numpad -
        case VK_NUMPAD4:
            scancode = 0x4B;
            break;   // Numpad 4
        case VK_NUMPAD5:
            scancode = 0x4C;
            break;   // Numpad 5
        case VK_NUMPAD6:
            scancode = 0x4D;
            break;   // Numpad 6
        case VK_ADD:
            scancode = 0x4E;
            break;       // Numpad +
        case VK_NUMPAD1:
            scancode = 0x4F;
            break;   // Numpad 1
        case VK_NUMPAD2:
            scancode = 0x50;
            break;   // Numpad 2
        case VK_NUMPAD3:
            scancode = 0x51;
            break;   // Numpad 3
        case VK_NUMPAD0:
            scancode = 0x52;
            break;   // Numpad 0
        case VK_DECIMAL:
            scancode = 0x53;
            break;   // Numpad .

            // Additional keys (insert, delete, etc.)
        case VK_INSERT:
            scancode = 0x52;
            break;    // Insert
        case VK_DELETE:
            scancode = 0x53;
            break;    // Delete
        case VK_HOME:
            scancode = 0x47;
            break;      // Home
        case VK_END:
            scancode = 0x4F;
            break;       // End
        case VK_PRIOR:
            scancode = 0x49;
            break;     // Page Up
        case VK_NEXT:
            scancode = 0x51;
            break;      // Page Down
        case VK_UP:
            scancode = 0x48;
            break;        // Up Arrow
        case VK_DOWN:
            scancode = 0x50;
            break;      // Down Arrow
        case VK_LEFT:
            scancode = 0x4B;
            break;      // Left Arrow
        case VK_RIGHT:
            scancode = 0x4D;
            break;     // Right Arrow

        default:
            scancode = 0;
            break;  // No translation
    }

    // For key release (KeyUp), add 0x80 to scancode
    if (!isKeyDown && scancode != 0) {
        scancode |= 0x80;
    }

    port60 = scancode;
    port64 |= 2;

    doirq(1);
    //printf("scancode %c", scancode);
}

#define QUEUE_SIZE 100

typedef struct {
    uint16_t messages[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cvNotEmpty;
    CONDITION_VARIABLE cvNotFull;
} MessageQueue;

MessageQueue queue;
HANDLE hThread;

DWORD WINAPI MessageHandler(LPVOID lpParam);

// Initialize the message queue
void InitQueue(MessageQueue* q) {
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    InitializeCriticalSection(&q->cs);
    InitializeConditionVariable(&q->cvNotEmpty);
    InitializeConditionVariable(&q->cvNotFull);
}

// Destroy the message queue
void DestroyQueue(MessageQueue* q) {
    DeleteCriticalSection(&q->cs);
}

// Add a message to the queue
void Enqueue(MessageQueue* q, uint16_t message) {
    EnterCriticalSection(&q->cs);

    // Wait if the queue is full
    while (q->count == QUEUE_SIZE) {
        SleepConditionVariableCS(&q->cvNotFull, &q->cs, INFINITE);
    }

    q->messages[q->rear] = message;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->count++;

    // Signal that the queue is not empty
    WakeConditionVariable(&q->cvNotEmpty);

    LeaveCriticalSection(&q->cs);
}

// Remove a message from the queue
uint16_t Dequeue(MessageQueue* q) {
    EnterCriticalSection(&q->cs);

    // Wait if the queue is empty
    while (q->count == 0) {
        SleepConditionVariableCS(&q->cvNotEmpty, &q->cs, INFINITE);
    }

    uint16_t message = q->messages[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;

    // Signal that the queue is not full
    WakeConditionVariable(&q->cvNotFull);

    LeaveCriticalSection(&q->cs);

    return message;
}

// Thread function to handle the message queue
DWORD WINAPI MessageHandler(LPVOID lpParam) {
    MessageQueue* q = (MessageQueue*)lpParam;

    while (1) {
        uint16_t message = Dequeue(q);

        // Process the message (just print it for now)
//        printf("Message received: %u\n", message);
        if(hComm != NULL && !WriteFile(hComm, &message, 2, &bytesWritten, NULL)) {
//            printf("!!!! Error in writing to serial port\n");
        }
        // Exit the thread if we receive a special "exit" message
        if (message == 0xFFFF) {
            //break;
        }
    }

    return 0;
}

extern "C" void tandy_write(uint16_t reg, uint8_t value) {
    Enqueue(&queue, (value & 0xff) << 8 | 0);
}
extern "C" void adlib_write(uint16_t reg, uint8_t value) {
//    printf("Adlib Write %x %x", reg, value);
    uint16_t data = (reg & 0xff) << 8 | 2 << 4 | 0b0000 | (reg >> 8) & 1;
    Enqueue(&queue, data);
//    if(hComm != NULL && !WriteFile(hComm, &data, 2, &bytesWritten, NULL)) {
//        printf("!!!! Error in writing to serial port\n");
//    }

    data = (value & 0xff) << 8 | 2 << 4 | 0b0010 | (reg >> 8) & 1;
    Enqueue(&queue, data);
//    if(hComm != NULL && !WriteFile(hComm, &data, 2, &bytesWritten, NULL)) {
//        printf("!!!! Error in writing to serial port\n");
//    }
}
extern "C" void cms_write(uint16_t reg, uint8_t val) {
    switch (reg - 0x220) {
        case 0: Enqueue(&queue, (val & 0xff) << 8 | 3 << 4 | 0b0000); break;
        case 1: Enqueue(&queue, (val & 0xff) << 8 | 3 << 4 | 0b0010); break;
        case 2: Enqueue(&queue, (val & 0xff) << 8 | 3 << 4 | 0b0001); break;
        case 3: Enqueue(&queue, (val & 0xff) << 8 | 3 << 4 | 0b0011); break;

    }
}


int main(int argc, char **argv) {
    int scale = 2;

    if (!mfb_open("PC", 640, 400, scale))
        return 1;

    // Initialize the message queue
    InitQueue(&queue);

    // Create the message handling thread
    hThread = CreateThread(NULL, 0, MessageHandler, &queue, 0, NULL);
    if (hThread == NULL) {
        printf("Error creating thread\n");
        return 1;
    }

/*    HANDLE sound_thread = CreateThread(NULL, 0, SoundThread, NULL, 0, NULL);
    if (!sound_thread)
        return 1;*/

    if(hComm == NULL) {
        // Open the serial port
        hComm = CreateFile("\\\\.\\COM7",
                           GENERIC_WRITE | GENERIC_READ,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);


        if(hComm == INVALID_HANDLE_VALUE) {
            printf("!!!! Error in opening serial port\n");
        }
        // Initialize the DCB structure
        SecureZeroMemory(&dcb, sizeof(DCB));
        dcb.DCBlength = sizeof(DCB);

        Sleep(10);
        // Get the current state
        BOOL fSuccess = GetCommState(hComm, &dcb);
        if(!fSuccess) {
            // Handle the error
            printf("!!!! Error in getting current serial port state\n");
            CloseHandle(hComm);
            //return 1;
        }

        // Set the new state
        dcb.BaudRate = CBR_115200;    // set the baud rate
        dcb.ByteSize = 8;           // data size, xmit, and rcv
        dcb.Parity = NOPARITY;    // no parity bit
        dcb.StopBits = ONESTOPBIT;  // one stop bit
        dcb.fDtrControl = DTR_CONTROL_ENABLE;


        fSuccess = SetCommState(hComm, &dcb);
        if(!fSuccess) {
            // Handle the error
            printf("!!!! Error in setting serial port state\n");
            CloseHandle(hComm);
            //return 1;
        }
        COMMTIMEOUTS timeouts;
        // Set the timeouts
        timeouts.ReadIntervalTimeout = 5;
        timeouts.ReadTotalTimeoutConstant = 5;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        timeouts.WriteTotalTimeoutConstant = 5;
        timeouts.WriteTotalTimeoutMultiplier = 1;
        Sleep(10);
        fSuccess = SetCommTimeouts(hComm, &timeouts);
        if(!fSuccess) {
            // Handle the error
            printf("!!!! Error in setting timeouts\n");
            CloseHandle(hComm);
            //return 1;
        }
        Sleep(10);
    }

    reset86();


    mfb_set_pallete((uint32_t *) cga_palette, 0, 16);

    mfb_set_pallete((uint32_t *) cga_composite_palette[0], 16, 32);
    mfb_set_pallete((uint32_t *) cga_composite_palette[1], 32, 48);
    mfb_set_pallete((uint32_t *) cga_composite_palette[2], 48, 64);

    mfb_set_pallete((uint32_t *) tga_palette, 64, 80);

    CreateThread(NULL, 0, TicksThread, NULL, 0, NULL);
    while (true) {

        exec86(327680);
        if (mfb_update(SCREEN, 0) == -1)
            exit(1);
    }
    // Wait for the thread to finish
//    WaitForSingleObject(hThread, INFINITE);

    // Clean up
    CloseHandle(hThread);
    DestroyQueue(&queue);
//    CloseHandle(sound_thread);
    mfb_close();
}