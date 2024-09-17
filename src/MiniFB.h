#ifndef _MINIFB_H_
#define _MINIFB_H_
#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MFB_RGB(r, g, b) (((unsigned int)r) << 16) | (((unsigned int)g) << 8) | b

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Create a window that is used to display the buffer sent into the mfb_update function, returns 0 if fails
int mfb_open(const char* name, int width, int height, int scale);

// Update the display. Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return -1 when ESC key is pressed (later on will return keycode and -1 on other close signal) 
int mfb_update(void* buffer, int fps_limit);
void mfb_set_pallete_array(const uint32_t *new_palette, uint8_t start, uint8_t count);
void mfb_set_pallete(const uint8_t color_index, const uint32_t color);
// Close the window
void mfb_close();
char * mfb_keystatus();
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
