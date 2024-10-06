#ifndef _EMUADPCM_H_
#define _EMUADPCM_H_

#include <stdint.h>

typedef struct __OPL_ADPCM {
  uint32_t clk;

  uint8_t reg[0x20];

  uint8_t *wave;      /* ADPCM DATA */
  uint8_t *memory[2]; /* [0] RAM, [1] ROM */

  uint8_t status;

  uint32_t start_addr;
  uint32_t stop_addr;
  uint32_t play_addr;  /* Current play address * 2 */
  uint32_t delta_addr; /* 16bit address */
  uint32_t delta_n;
  uint32_t play_addr_mask;

  uint8_t play_start;

  int32_t output[2];
  uint32_t diff;

} OPL_ADPCM;

OPL_ADPCM *OPL_ADPCM_new(uint32_t clk);
void OPL_ADPCM_reset(OPL_ADPCM *);
void OPL_ADPCM_delete(OPL_ADPCM *);
void OPL_ADPCM_writeReg(OPL_ADPCM *, uint32_t reg, uint32_t val);
int16_t OPL_ADPCM_calc(OPL_ADPCM *);
uint8_t OPL_ADPCM_status(OPL_ADPCM *);
void OPL_ADPCM_resetStatus(OPL_ADPCM *);
void OPL_ADPCM_writeRAM(OPL_ADPCM *, uint32_t start, uint32_t length, const uint8_t *data);
void OPL_ADPCM_writeROM(OPL_ADPCM *, uint32_t start, uint32_t length, const uint8_t *data);
#endif
