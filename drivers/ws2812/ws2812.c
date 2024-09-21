#include "ws2812.h"
#include "ws2812.pio.h"


void ws2812_set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
  pio_sm_restart(WS2812_PIO, WS2812_SM);

  // write the rgb
  uint32_t mask = (green << 16) | (red << 8) | (blue << 0);
  pio_sm_put_blocking(WS2812_PIO, WS2812_SM, mask << 8u);
}

void ws2812_init() {
  ws2812_program_init(WS2812_PIO, WS2812_SM, WS2812_PIN, true);
}

void ws2812_reset() {
  // turn it off
  ws2812_set_rgb(0, 0, 0);
  pio_sm_set_enabled(WS2812_PIO, WS2812_SM, false);
  pio_sm_restart(WS2812_PIO, WS2812_SM);
}
