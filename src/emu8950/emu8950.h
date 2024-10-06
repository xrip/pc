#ifndef _EMU8950_H_
#define _EMU8950_H_

#include "emuadpcm.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPL_DEBUG 0

/* voice data */
typedef struct __OPL_PATCH {
  uint8_t TL, FB, EG, ML, AR, DR, SL, RR, KR, KL, AM, PM, WS;
} OPL_PATCH;

/* mask */
#define OPL_MASK_CH(x) (1 << (x))
#define OPL_MASK_HH (1 << 9)
#define OPL_MASK_CYM (1 << 10)
#define OPL_MASK_TOM (1 << 11)
#define OPL_MASK_SD (1 << 12)
#define OPL_MASK_BD (1 << 13)
#define OPL_MASK_ADPCM (1 << 14)
#define OPL_MASK_RHYTHM (OPL_MASK_HH | OPL_MASK_CYM | OPL_MASK_TOM | OPL_MASK_SD | OPL_MASK_BD)

/* rate conveter */
typedef struct __OPL_RateConv {
  int ch;
  double timer;
  double f_ratio;
  int16_t *sinc_table;
  int16_t **buf;
} OPL_RateConv;

OPL_RateConv *OPL_RateConv_new(double f_inp, double f_out, int ch);
void OPL_RateConv_reset(OPL_RateConv *conv);
void OPL_RateConv_putData(OPL_RateConv *conv, int ch, int16_t data);
int16_t OPL_RateConv_getData(OPL_RateConv *conv, int ch);
void OPL_RateConv_delete(OPL_RateConv *conv);

/* slot */
typedef struct __OPL_SLOT {
  uint8_t number;

  /* type flags:
   * 000000SM 
   *       |+-- M: 0:modulator 1:carrier
   *       +--- S: 0:normal 1:single slot mode (sd, tom, hh or cym) 
   */
  uint8_t type;

  OPL_PATCH __patch;  
  OPL_PATCH *patch;  /* = alias for __patch */

  /* slot output */
  int32_t output[2]; /* output value, latest and previous. */

  /* phase generator (pg) */
  uint16_t *wave_table; /* wave table */
  uint32_t pg_phase;    /* pg phase */
  uint32_t pg_out;      /* pg output, as index of wave table */
  uint8_t pg_keep;      /* if 1, pg_phase is preserved when key-on */
  uint16_t blk_fnum;    /* (block << 9) | f-number */
  uint16_t fnum;        /* f-number (9 bits) */
  uint8_t blk;          /* block (3 bits) */

  /* envelope generator (eg) */
  uint8_t eg_state;         /* current state */
  uint16_t tll;             /* total level + key scale level*/
  uint8_t rks;              /* key scale offset (rks) for eg speed */
  uint8_t eg_rate_h;        /* eg speed rate high 4bits */
  uint8_t eg_rate_l;        /* eg speed rate low 2bits */
  uint32_t eg_shift;        /* shift for eg global counter, controls envelope speed */
  int16_t eg_out;           /* eg output */

  uint32_t update_requests; /* flags to debounce update */

#if OPL_DEBUG
  uint8_t last_eg_state;
#endif
} OPL_SLOT;

typedef struct __OPL {
  OPL_ADPCM *adpcm;
  uint32_t clk;
  uint32_t rate;

  uint8_t chip_type;

  uint32_t adr;

  uint8_t csm_mode;
  uint8_t csm_key_count;
  uint8_t notesel;

  uint32_t inp_step;
  uint32_t out_step;
  uint32_t out_time;

  uint8_t reg[0x100];
  uint8_t test_flag;
  uint32_t slot_key_status;
  uint8_t rhythm_mode;

  uint32_t eg_counter;

  uint32_t pm_phase;
  uint32_t pm_dphase;

  int32_t am_phase;
  int32_t am_dphase;
  uint8_t lfo_am;

  uint32_t noise;
  uint8_t short_noise;

  OPL_SLOT slot[18];
  uint8_t ch_alg[9]; // alg for each channels

  uint8_t pan[16];
  float pan_fine[16][2];

  uint32_t mask;
  uint8_t am_mode;
  uint8_t pm_mode;

  /* channel output */
  /* 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14:adpcm */
  int16_t ch_out[15];

  int16_t mix_out[2];

  OPL_RateConv *conv;

  uint32_t timer1_counter; //  80us counter
  uint32_t timer2_counter; // 320us counter
  void *timer1_user_data;
  void *timer2_user_data;
  void (*timer1_func)(void *user);
  void (*timer2_func)(void *user);
  uint8_t status;

} OPL;

OPL *OPL_new(uint32_t clk, uint32_t rate);
void OPL_delete(OPL *);

void OPL_reset(OPL *);

/** 
 * Set output wave sampling rate. 
 * @param rate sampling rate. If clock / 72 (typically 49716 or 49715 at 3.58MHz) is set, the internal rate converter is disabled.
 */
void OPL_setRate(OPL *opl, uint32_t rate);

/** 
 * Set internal calcuration quality. Currently no effects, just for compatibility.
 * >= v1.0.0 always synthesizes internal output at clock/72 Hz.
 */
void OPL_setQuality(OPL *opl, uint8_t q);

/**
 * Set OPL chip type.
 * @param type 0:Y8950, 1:YM3526, 2:YM3812
 */
void OPL_setChipType(OPL *opl, uint8_t type);

/** 
 * Set pan pot (extra function - not YM2413 chip feature)
 * @param ch 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14,15:reserved
 * @param pan 0:mute 1:right 2:left 3:center 
 * ```
 * pan: 76543210
 *            |+- bit 1: enable Left output
 *            +-- bit 0: enable Right output
 * ```
 */
void OPL_setPan(OPL *opl, uint32_t ch, uint8_t pan);

/**
 * Set fine-grained panning
 * @param ch 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14,15:reserved
 * @param pan output strength of left/right channel. 
 *            pan[0]: left, pan[1]: right. pan[0]=pan[1]=1.0f for center.
 */
void OPL_setPanFine(OPL *opl, uint32_t ch, float pan[2]);

void OPL_writeIO(OPL *opl, uint32_t reg, uint8_t val);
void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t val);

/**
 * Calculate sample
 */
int16_t OPL_calc(OPL *opl);

/**
 * Calulate stereo sample
 */
void OPL_calcStereo(OPL *opl, int32_t out[2]);

/** 
 *  Set channel mask 
 *  @param mask mask flag: OPL_MASK_* can be used.
 *  - bit 0..8: mask for ch 1 to 9 (OPL_MASK_CH(i))
 *  - bit 9: mask for Hi-Hat (OPL_MASK_HH)
 *  - bit 10: mask for Top-Cym (OPL_MASK_CYM)
 *  - bit 11: mask for Tom (OPL_MASK_TOM)
 *  - bit 12: mask for Snare Drum (OPL_MASK_SD)
 *  - bit 13: mask for Bass Drum (OPL_MASK_BD)
 */
uint32_t OPL_setMask(OPL *, uint32_t mask);

/**
 * Toggler channel mask flag
 */
uint32_t OPL_toggleMask(OPL *, uint32_t mask);

uint8_t OPL_readIO(OPL *opl);

/**
 * Read OPL status register
 * @returns
 * 76543210
 * |||||  +- D0: PCM/BSY
 * ||||+---- D3: BUF/RDY
 * |||+----- D4: EOS
 * ||+------ D5: TIMER2
 * |+------- D6: TIMER1
 * +-------- D7: IRQ
 */
uint8_t OPL_status(OPL *opl);

void OPL_writeADPCMData(OPL *opl, uint8_t type, uint32_t start, uint32_t length, const uint8_t *data);

/* for compatibility */
#define OPL_set_rate OPL_setRate
#define OPL_set_quality OPL_setQuality
#define OPL_set_pan OPL_setPan
#define OPL_set_pan_fine OPL_setPanFine
#define OPL_calc_stereo OPL_calcStereo

#ifdef __cplusplus
}
#endif

#endif
