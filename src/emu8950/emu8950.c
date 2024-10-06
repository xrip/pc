/**
 * emu8950 v1.1.3
 * https://github.com/digital-sound-antiques/emu8950
 * Copyright (C) 2001-2020 Mitsutaka Okazaki
 */
#include "emu8950.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE __inline__
#else
#define INLINE inline
#endif
#endif

#define _PI_ 3.14159265358979323846264338327950288

enum __OPL_EG_STATE { ATTACK, DECAY, SUSTAIN, RELEASE, UNKNOWN };
enum __OPL_TYPE { TYPE_Y8950 = 0, TYPE_YM3526, TYPE_YM3812, TYPE_MAX };

/* phase increment counter */
#define DP_BITS 20
#define DP_WIDTH (1 << DP_BITS)
#define DP_BASE_BITS (DP_BITS - PG_BITS)

/* dynamic range of envelope output */
#define EG_STEP 0.1875
#define EG_BITS 9
#define EG_MUTE ((1 << EG_BITS) - 1)
#define EG_MAX (0x1f0) // 93dB

/* dynamic range of total level */
#define TL_STEP 0.75
#define TL_BITS 6

/* dynamic range of sustine level */
#define SL_STEP 3.0
#define SL_BITS 4

#define TL2EG(tl) ((tl) << 2)

/* sine table */
#define PG_BITS 10 /* 2^10 = 1024 length sine table */
#define PG_WIDTH (1 << PG_BITS)

/* clang-format off */
/* exp_table[x] = round((exp2((double)x / 256.0) - 1) * 1024) */
static uint16_t exp_table[256] = {
0,    3,    6,    8,    11,   14,   17,   20,   22,   25,   28,   31,   34,   37,   40,   42,
45,   48,   51,   54,   57,   60,   63,   66,   69,   72,   75,   78,   81,   84,   87,   90,
93,   96,   99,   102,  105,  108,  111,  114,  117,  120,  123,  126,  130,  133,  136,  139,
142,  145,  148,  152,  155,  158,  161,  164,  168,  171,  174,  177,  181,  184,  187,  190,
194,  197,  200,  204,  207,  210,  214,  217,  220,  224,  227,  231,  234,  237,  241,  244,
248,  251,  255,  258,  262,  265,  268,  272,  276,  279,  283,  286,  290,  293,  297,  300,
304,  308,  311,  315,  318,  322,  326,  329,  333,  337,  340,  344,  348,  352,  355,  359,
363,  367,  370,  374,  378,  382,  385,  389,  393,  397,  401,  405,  409,  412,  416,  420,
424,  428,  432,  436,  440,  444,  448,  452,  456,  460,  464,  468,  472,  476,  480,  484,
488,  492,  496,  501,  505,  509,  513,  517,  521,  526,  530,  534,  538,  542,  547,  551,
555,  560,  564,  568,  572,  577,  581,  585,  590,  594,  599,  603,  607,  612,  616,  621,
625,  630,  634,  639,  643,  648,  652,  657,  661,  666,  670,  675,  680,  684,  689,  693,
698,  703,  708,  712,  717,  722,  726,  731,  736,  741,  745,  750,  755,  760,  765,  770,
774,  779,  784,  789,  794,  799,  804,  809,  814,  819,  824,  829,  834,  839,  844,  849,
854,  859,  864,  869,  874,  880,  885,  890,  895,  900,  906,  911,  916,  921,  927,  932,
937,  942,  948,  953,  959,  964,  969,  975,  980,  986,  991,  996, 1002, 1007, 1013, 1018
};
/* logsin_table[x] = round(-log2(sin((x + 0.5) * PI / (PG_WIDTH / 4) / 2)) * 256) */
static uint16_t logsin_table[PG_WIDTH / 4] = {
2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013, 979,  949,  920,  894,  869, 
846,  825,  804,  785,  767,  749,  732,  717,  701,  687,  672,  659,  646,  633,  621,  609, 
598,  587,  576,  566,  556,  546,  536,  527,  518,  509,  501,  492,  484,  476,  468,  461,
453,  446,  439,  432,  425,  418,  411,  405,  399,  392,  386,  380,  375,  369,  363,  358,  
352,  347,  341,  336,  331,  326,  321,  316,  311,  307,  302,  297,  293,  289,  284,  280,
276,  271,  267,  263,  259,  255,  251,  248,  244,  240,  236,  233,  229,  226,  222,  219, 
215,  212,  209,  205,  202,  199,  196,  193,  190,  187,  184,  181,  178,  175,  172,  169, 
167,  164,  161,  159,  156,  153,  151,  148,  146,  143,  141,  138,  136,  134,  131,  129,  
127,  125,  122,  120,  118,  116,  114,  112,  110,  108,  106,  104,  102,  100,  98,   96,   
94,   92,   91,   89,   87,   85,   83,   82,   80,   78,   77,   75,   74,   72,   70,   69,
67,   66,   64,   63,   62,   60,   59,   57,   56,   55,   53,   52,   51,   49,   48,   47,  
46,   45,   43,   42,   41,   40,   39,   38,   37,   36,   35,   34,   33,   32,   31,   30,  
29,   28,   27,   26,   25,   24,   23,   23,   22,   21,   20,   20,   19,   18,   17,   17,   
16,   15,   15,   14,   13,   13,   12,   12,   11,   10,   10,   9,    9,    8,    8,    7,    
7,    7,    6,    6,    5,    5,    5,    4,    4,    4,    3,    3,    3,    2,    2,    2,
2,    1,    1,    1,    1,    1,    1,    1,    0,    0,    0,    0,    0,    0,    0,    0,
};
/* clang-format on */

static uint16_t wave_table_map[4][PG_WIDTH];

/* pitch modulator */
#define PM_PG_BITS 3
#define PM_PG_WIDTH (1 << PM_PG_BITS)
#define PM_DP_BITS 22
#define PM_DP_WIDTH (1 << PM_DP_BITS)

/* offset to fnum, rough approximation of 14 cents depth. */
static int8_t pm_table[8][PM_PG_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0},    // fnum = 000xxxxx
    {0, 0, 1, 0, 0, 0, -1, 0},   // fnum = 001xxxxx
    {0, 1, 2, 1, 0, -1, -2, -1}, // fnum = 010xxxxx
    {0, 1, 3, 1, 0, -1, -3, -1}, // fnum = 011xxxxx
    {0, 2, 4, 2, 0, -2, -4, -2}, // fnum = 100xxxxx
    {0, 2, 5, 2, 0, -2, -5, -2}, // fnum = 101xxxxx
    {0, 3, 6, 3, 0, -3, -6, -3}, // fnum = 110xxxxx
    {0, 3, 7, 3, 0, -3, -7, -3}, // fnum = 111xxxxx
};

/* amplitude lfo table */
/* The following envelop pattern is verified on real YM2413. */
/* each element repeates 64 cycles */
static uint8_t am_table[210] = {0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  //
                                2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  //
                                4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  //
                                6,  6,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  //
                                8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,  //
                                10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, //
                                12, 12, 12, 12, 12, 12, 12, 12,                                 //
                                13, 13, 13,                                                     //
                                12, 12, 12, 12, 12, 12, 12, 12,                                 //
                                11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, //
                                9,  9,  9,  9,  9,  9,  9,  9,  8,  8,  8,  8,  8,  8,  8,  8,  //
                                7,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6,  6,  //
                                5,  5,  5,  5,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  4,  //
                                3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,  2,  2,  //
                                1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0};

/* envelope decay increment step table */
static uint8_t eg_step_tables[4][8] = {
    {0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 1, 1, 0, 1},
    {0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1},
};
static uint8_t eg_step_tables_fast[4][8] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 2, 1, 1, 1, 2},
    {1, 2, 1, 2, 1, 2, 1, 2},
    {1, 2, 2, 2, 1, 2, 2, 2},
};

static uint32_t ml_table[16] = {1,     1 * 2, 2 * 2,  3 * 2,  4 * 2,  5 * 2,  6 * 2,  7 * 2,
                                8 * 2, 9 * 2, 10 * 2, 10 * 2, 12 * 2, 12 * 2, 15 * 2, 15 * 2};

#define dB2(x) ((x) * 2)
static double kl_table[16] = {dB2(0.000),  dB2(9.000),  dB2(12.000), dB2(13.875), dB2(15.000), dB2(16.125),
                              dB2(16.875), dB2(17.625), dB2(18.000), dB2(18.750), dB2(19.125), dB2(19.500),
                              dB2(19.875), dB2(20.250), dB2(20.625), dB2(21.000)};

static uint32_t tll_table[8 * 16][1 << TL_BITS][4];
static int32_t rks_table[2][32][2];

#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

/***************************************************

           Internal Sample Rate Converter

****************************************************/
/* Note: to disable internal rate converter, set clock/72 to output sampling rate. */

/*
 * LW is truncate length of sinc(x) calculation.
 * Lower LW is faster, higher LW results better quality.
 * LW must be a non-zero positive even number, no upper limit.
 * LW=16 or greater is recommended when upsampling.
 * LW=8 is practically okay for downsampling.
 */
#define LW 16

/* resolution of sinc(x) table. sinc(x) where 0.0<=x<1.0 corresponds to sinc_table[0...SINC_RESO-1] */
#define SINC_RESO 256
#define SINC_AMP_BITS 12

// double hamming(double x) { return 0.54 - 0.46 * cos(2 * PI * x); }
static double blackman(double x) { return 0.42 - 0.5 * cos(2 * _PI_ * x) + 0.08 * cos(4 * _PI_ * x); }
static double sinc(double x) { return (x == 0.0 ? 1.0 : sin(_PI_ * x) / (_PI_ * x)); }
static double windowed_sinc(double x) { return blackman(0.5 + 0.5 * x / (LW / 2)) * sinc(x); }

/* f_inp: input frequency. f_out: output frequencey, ch: number of channels */
OPL_RateConv *OPL_RateConv_new(double f_inp, double f_out, int ch) {
  OPL_RateConv *conv = malloc(sizeof(OPL_RateConv));
  int i;

  conv->ch = ch;
  conv->f_ratio = f_inp / f_out;
  conv->buf = malloc(sizeof(void *) * ch);
  for (i = 0; i < ch; i++) {
    conv->buf[i] = malloc(sizeof(conv->buf[0][0]) * LW);
  }

  /* create sinc_table for positive 0 <= x < LW/2 */
  conv->sinc_table = malloc(sizeof(conv->sinc_table[0]) * SINC_RESO * LW / 2);
  for (i = 0; i < SINC_RESO * LW / 2; i++) {
    const double x = (double)i / SINC_RESO;
    if (f_out < f_inp) {
      /* for downsampling */
      conv->sinc_table[i] = (int16_t)((1 << SINC_AMP_BITS) * windowed_sinc(x / conv->f_ratio) / conv->f_ratio);
    } else {
      /* for upsampling */
      conv->sinc_table[i] = (int16_t)((1 << SINC_AMP_BITS) * windowed_sinc(x));
    }
  }

  return conv;
}

static INLINE int16_t lookup_sinc_table(int16_t *table, double x) {
  int16_t index = (int16_t)(x * SINC_RESO);
  if (index < 0)
    index = -index;
  return table[min(SINC_RESO * LW / 2 - 1, index)];
}

void OPL_RateConv_reset(OPL_RateConv *conv) {
  int i;
  conv->timer = 0;
  for (i = 0; i < conv->ch; i++) {
    memset(conv->buf[i], 0, sizeof(conv->buf[i][0]) * LW);
  }
}

/* put original data to this converter at f_inp. */
void OPL_RateConv_putData(OPL_RateConv *conv, int ch, int16_t data) {
  int16_t *buf = conv->buf[ch];
  int i;
  for (i = 0; i < LW - 1; i++) {
    buf[i] = buf[i + 1];
  }
  buf[LW - 1] = data;
}

/* get resampled data from this converter at f_out. */
/* this function must be called f_out / f_inp times per one putData call. */
int16_t OPL_RateConv_getData(OPL_RateConv *conv, int ch) {
  int16_t *buf = conv->buf[ch];
  int32_t sum = 0;
  int k;
  double dn;
  conv->timer += conv->f_ratio;
  dn = conv->timer - floor(conv->timer);
  conv->timer = dn;

  for (k = 0; k < LW; k++) {
    double x = ((double)k - (LW / 2 - 1)) - dn;
    sum += buf[k] * lookup_sinc_table(conv->sinc_table, x);
  }
  return sum >> SINC_AMP_BITS;
}

void OPL_RateConv_delete(OPL_RateConv *conv) {
  int i;
  for (i = 0; i < conv->ch; i++) {
    free(conv->buf[i]);
  }
  free(conv->buf);
  free(conv->sinc_table);
  free(conv);
}

/***************************************************

                  Create tables

****************************************************/
static void makeSinTable(void) {
  int x;

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 4) {
      wave_table_map[0][x] = logsin_table[x];
    } else if (x < PG_WIDTH / 2) {
      wave_table_map[0][x] = logsin_table[PG_WIDTH / 2 - x - 1];
    } else {
      wave_table_map[0][x] = 0x8000 | wave_table_map[0][PG_WIDTH - x - 1];
    }
  }

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 2) {
      wave_table_map[1][x] = wave_table_map[0][x];
    } else {
      wave_table_map[1][x] = 0xfff;
    }
  }

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 2) {
      wave_table_map[2][x] = wave_table_map[0][x];
    } else {
      wave_table_map[2][x] = wave_table_map[0][x - PG_WIDTH / 2];
    }
  }

  for (x = 0; x < PG_WIDTH; x++) {
    if (x < PG_WIDTH / 4) {
      wave_table_map[3][x] = wave_table_map[0][x];
    } else if (x < PG_WIDTH / 2) {
      wave_table_map[3][x] = 0xfff;
    } else if (x < PG_WIDTH * 3 / 4) {
      wave_table_map[3][x] = wave_table_map[0][x - PG_WIDTH / 2];
    } else {
      wave_table_map[3][x] = 0xfff;
    }
  }
}

static void makeTllTable(void) {

  int32_t tmp;
  int32_t fnum, block, TL, KL, kx;

  for (fnum = 0; fnum < 16; fnum++) {
    for (block = 0; block < 8; block++) {
      for (TL = 0; TL < 64; TL++) {
        for (KL = 0; KL < 4; KL++) {
          kx = ((KL & 1) << 1) | ((KL >> 1) & 1);
          if (KL == 0) {
            tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
          } else {
            tmp = (int32_t)(kl_table[fnum] - dB2(3.000) * (7 - block));
            if (tmp <= 0)
              tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
            else
              tll_table[(block << 4) | fnum][TL][KL] = (uint32_t)((tmp >> (3 - kx)) / EG_STEP) + TL2EG(TL);
          }
        }
      }
    }
  }
}

static void makeRksTable(void) {
  int fnum8, fnum9, blk;
  int blk_fnum98;
  for (fnum8 = 0; fnum8 < 2; fnum8++)
    for (fnum9 = 0; fnum9 < 2; fnum9++)
      for (blk = 0; blk < 8; blk++) {
        blk_fnum98 = (blk << 2) | (fnum9 << 1) | fnum8;
        rks_table[0][blk_fnum98][1] = (blk << 1) + fnum9;
        rks_table[0][blk_fnum98][0] = blk >> 1;
        rks_table[1][blk_fnum98][1] = (blk << 1) + (fnum9 & fnum8);
        rks_table[1][blk_fnum98][0] = blk >> 1;
      }
}

static uint8_t table_initialized = 0;

static void initializeTables() {
  makeTllTable();
  makeRksTable();
  makeSinTable();
  table_initialized = 1;
}

/*********************************************************

                      Synthesizing

*********************************************************/
#define SLOT_BD1 12
#define SLOT_BD2 13
#define SLOT_HH 14
#define SLOT_SD 15
#define SLOT_TOM 16
#define SLOT_CYM 17

/* utility macros */
#define MOD(o, x) (&(o)->slot[(x) << 1])
#define CAR(o, x) (&(o)->slot[((x) << 1) | 1])
#define BIT(s, b) (((s) >> (b)) & 1)

#if OPL_DEBUG
static void _debug_print_patch(OPL_SLOT *slot) {
  OPL_PATCH *p = slot->patch;
  printf("[slot#%d am:%d pm:%d eg:%d kr:%d ml:%d kl:%d tl:%d ws:%d fb:%d A:%d D:%d S:%d R:%d]\n", slot->number, //
         p->AM, p->PM, p->EG, p->KR, p->ML,                                                                     //
         p->KL, p->TL, p->WS, p->FB,                                                                            //
         p->AR, p->DR, p->SL, p->RR);
}

static char *_debug_eg_state_name(OPL_SLOT *slot) {
  switch (slot->eg_state) {
  case ATTACK:
    return "attack";
  case DECAY:
    return "decay";
  case SUSTAIN:
    return "sustain";
  case RELEASE:
    return "release";
  default:
    return "unknown";
  }
}

static INLINE void _debug_print_slot_info(OPL_SLOT *slot) {
  char *name = _debug_eg_state_name(slot);
  _debug_print_patch(slot);
  printf("[slot#%d state:%s fnum:%03x rate:%d-%d]\n", slot->number, name, slot->blk_fnum, slot->eg_rate_h,
         slot->eg_rate_l);
  fflush(stdout);
}
#endif

static INLINE int get_parameter_rate(OPL_SLOT *slot) {
  switch (slot->eg_state) {
  case ATTACK:
    return slot->patch->AR;
  case DECAY:
    return slot->patch->DR;
  case SUSTAIN:
    return slot->patch->EG ? 0 : slot->patch->RR;
  case RELEASE:
    return slot->patch->RR;
  default:
    return 0;
  }
}

enum SLOT_UPDATE_FLAG {
  UPDATE_WS = 1,
  UPDATE_TLL = 2,
  UPDATE_RKS = 4,
  UPDATE_EG = 8,
  UPDATE_ALL = 255,
};

static INLINE void request_update(OPL_SLOT *slot, int flag) { slot->update_requests |= flag; }

static void commit_slot_update(OPL_SLOT *slot, uint8_t notesel) {

  if (slot->update_requests & UPDATE_WS) {
    slot->wave_table = wave_table_map[slot->patch->WS & 3];
  }

  if (slot->update_requests & UPDATE_TLL) {
    if ((slot->type & 1) == 0) {
      slot->tll = tll_table[slot->blk_fnum >> 6][slot->patch->TL][slot->patch->KL];
    } else {
      slot->tll = tll_table[slot->blk_fnum >> 6][slot->patch->TL][slot->patch->KL];
    }
  }

  if (slot->update_requests & UPDATE_RKS) {
    slot->rks = rks_table[notesel][slot->blk_fnum >> 8][slot->patch->KR];
  }

  if (slot->update_requests & (UPDATE_RKS | UPDATE_EG)) {
    int p_rate = get_parameter_rate(slot);

    if (p_rate == 0) {
      slot->eg_shift = 0;
      slot->eg_rate_h = 0;
      slot->eg_rate_l = 0;
    } else {
      slot->eg_rate_h = min(15, p_rate + (slot->rks >> 2));
      slot->eg_rate_l = slot->rks & 3;
      if (slot->eg_state == ATTACK) {
        slot->eg_shift = (0 < slot->eg_rate_h && slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
      } else {
        slot->eg_shift = (slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
      }
    }
  }

#if OPL_DEBUG
  if (slot->last_eg_state != slot->eg_state) {
    _debug_print_slot_info(slot);
    slot->last_eg_state = slot->eg_state;
  }
#endif

  slot->update_requests = 0;
}

static void reset_slot(OPL_SLOT *slot, int number) {
  slot->patch = &(slot->__patch);
  memset(slot->patch, 0, sizeof(OPL_PATCH));
  slot->number = number;
  slot->type = number % 2;
  slot->pg_keep = 0;
  slot->wave_table = wave_table_map[0];
  slot->pg_phase = 0;
  slot->output[0] = 0;
  slot->output[1] = 0;
  slot->eg_state = RELEASE;
  slot->eg_shift = 0;
  slot->rks = 0;
  slot->tll = 0;
  slot->blk_fnum = 0;
  slot->blk = 0;
  slot->fnum = 0;
  slot->pg_out = 0;
  slot->eg_out = EG_MUTE;
}

static INLINE void slotOn(OPL *opl, int i) {
  OPL_SLOT *slot = &opl->slot[i];
  slot->rks = rks_table[opl->notesel][slot->blk_fnum >> 8][slot->patch->KR];
  if (min(15, slot->patch->AR + (slot->rks >> 2)) == 15) {
    slot->eg_state = DECAY;
    slot->eg_out = 0;
  } else {
    slot->eg_state = ATTACK;
  }
  if (!slot->pg_keep) {
    slot->pg_phase = 0;
  }
  request_update(slot, UPDATE_EG);
}

static INLINE void slotOff(OPL *opl, int i) {
  OPL_SLOT *slot = &opl->slot[i];
  slot->eg_state = RELEASE;
  request_update(slot, UPDATE_EG);
}

static INLINE void update_key_status(OPL *opl) {
  const uint8_t r14 = opl->reg[0xbd];
  const uint8_t rhythm_mode = BIT(r14, 5);
  uint32_t new_slot_key_status = 0;
  uint32_t updated_status;
  int ch;

  if (opl->csm_mode && opl->csm_key_count) {
    new_slot_key_status = 0x3ffff;
  }

  for (ch = 0; ch < 9; ch++)
    if (opl->reg[0xB0 + ch] & 0x20)
      new_slot_key_status |= 3 << (ch * 2);

  if (rhythm_mode) {
    if (r14 & 0x10)
      new_slot_key_status |= 3 << SLOT_BD1;

    if (r14 & 0x01)
      new_slot_key_status |= 1 << SLOT_HH;

    if (r14 & 0x08)
      new_slot_key_status |= 1 << SLOT_SD;

    if (r14 & 0x04)
      new_slot_key_status |= 1 << SLOT_TOM;

    if (r14 & 0x02)
      new_slot_key_status |= 1 << SLOT_CYM;
  }

  updated_status = opl->slot_key_status ^ new_slot_key_status;

  if (updated_status) {
    int i;
    for (i = 0; i < 18; i++)
      if (BIT(updated_status, i)) {
        if (BIT(new_slot_key_status, i)) {
          slotOn(opl, i);
        } else {
          slotOff(opl, i);
        }
      }
  }

  opl->slot_key_status = new_slot_key_status;
}

/* set f-Nnmber ( fnum : 10bit ) */
static INLINE void set_fnumber(OPL *opl, int ch, int fnum) {
  OPL_SLOT *car = CAR(opl, ch);
  OPL_SLOT *mod = MOD(opl, ch);
  car->fnum = fnum;
  car->blk_fnum = (car->blk_fnum & 0x1c00) | (fnum & 0x3ff);
  mod->fnum = fnum;
  mod->blk_fnum = (mod->blk_fnum & 0x1c00) | (fnum & 0x3ff);
  request_update(car, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
  request_update(mod, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
}

/* set block data (blk : 3bit ) */
static INLINE void set_block(OPL *opl, int ch, int blk) {
  OPL_SLOT *car = CAR(opl, ch);
  OPL_SLOT *mod = MOD(opl, ch);
  car->blk = blk;
  car->blk_fnum = ((blk & 7) << 10) | (car->blk_fnum & 0x3ff);
  mod->blk = blk;
  mod->blk_fnum = ((blk & 7) << 10) | (mod->blk_fnum & 0x3ff);
  request_update(car, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
  request_update(mod, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
}

static INLINE void update_rhythm_mode(OPL *opl) {
  const uint8_t new_rhythm_mode = (opl->reg[0xbd] >> 5) & 1;

  if (opl->rhythm_mode != new_rhythm_mode) {
    if (new_rhythm_mode) {
      opl->slot[SLOT_HH].type = 3;
      opl->slot[SLOT_HH].pg_keep = 1;
      opl->slot[SLOT_SD].type = 3;
      opl->slot[SLOT_TOM].type = 3;
      opl->slot[SLOT_CYM].type = 3;
      opl->slot[SLOT_CYM].pg_keep = 1;
    } else {
      opl->slot[SLOT_HH].type = 0;
      opl->slot[SLOT_HH].pg_keep = 0;
      opl->slot[SLOT_SD].type = 1;
      opl->slot[SLOT_TOM].type = 0;
      opl->slot[SLOT_CYM].type = 1;
      opl->slot[SLOT_CYM].pg_keep = 0;
    }
  }
  opl->rhythm_mode = new_rhythm_mode;
}

static void update_ampm(OPL *opl) {
  const uint32_t pm_inc = (opl->test_flag & 8) ? opl->pm_dphase << 10 : opl->pm_dphase;
  const uint32_t am_inc = (opl->test_flag & 8) ? 64 : 1;
  if (opl->test_flag & 2) {
    opl->pm_phase = 0;
    opl->am_phase = 0;
  } else {
    opl->pm_phase = (opl->pm_phase + pm_inc) & (PM_DP_WIDTH - 1);
    opl->am_phase += am_inc;
  }
  opl->lfo_am = am_table[(opl->am_phase >> 6) % sizeof(am_table)] >> (opl->am_mode ? 0 : 2);
}

static void update_noise(OPL *opl, int cycle) {
  int i;
  for (i = 0; i < cycle; i++) {
    if (opl->noise & 1) {
      opl->noise ^= 0x800200;
    }
    opl->noise >>= 1;
  }
}

static void update_short_noise(OPL *opl) {
  const uint32_t pg_hh = opl->slot[SLOT_HH].pg_out;
  const uint32_t pg_cym = opl->slot[SLOT_CYM].pg_out;

  const uint8_t h_bit2 = BIT(pg_hh, PG_BITS - 8);
  const uint8_t h_bit7 = BIT(pg_hh, PG_BITS - 3);
  const uint8_t h_bit3 = BIT(pg_hh, PG_BITS - 7);

  const uint8_t c_bit3 = BIT(pg_cym, PG_BITS - 7);
  const uint8_t c_bit5 = BIT(pg_cym, PG_BITS - 5);

  opl->short_noise = (h_bit2 ^ h_bit7) | (h_bit3 ^ c_bit5) | (c_bit3 ^ c_bit5);
}

static INLINE void calc_phase(OPL_SLOT *slot, int32_t pm_phase, uint8_t pm_mode, uint8_t reset) {
  int8_t pm = 0;
  if (slot->patch->PM) {
    pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
    pm >>= (pm_mode ? 0 : 1);
  }

  if (reset) {
    slot->pg_phase = 0;
  }
  slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
  slot->pg_phase &= (DP_WIDTH - 1);
  slot->pg_out = slot->pg_phase >> DP_BASE_BITS;
}

static INLINE uint8_t lookup_attack_step(OPL_SLOT *slot, uint32_t counter) {
  int index = (counter >> slot->eg_shift) & 7;
  switch (slot->eg_rate_h) {
  case 13:
    return eg_step_tables_fast[slot->eg_rate_l][index];
  case 14:
    return eg_step_tables_fast[slot->eg_rate_l][index] << 1;
  case 0:
  case 15:
    return 0;
  default:
    return eg_step_tables[slot->eg_rate_l][index];
  }
}

static INLINE uint8_t lookup_decay_step(OPL_SLOT *slot, uint32_t counter) {
  int index = (counter >> slot->eg_shift) & 7;
  switch (slot->eg_rate_h) {
  case 0:
    return 0;
  case 13:
    return eg_step_tables_fast[slot->eg_rate_l][index];
  case 14:
    return eg_step_tables_fast[slot->eg_rate_l][index] << 1;
  case 15:
    return 4;
  default:
    return eg_step_tables[slot->eg_rate_l][index];
  }
}

static INLINE void calc_envelope(OPL_SLOT *slot, uint16_t eg_counter, uint8_t test) {

  uint16_t mask = (1 << slot->eg_shift) - 1;
  uint8_t s;

  if (slot->eg_state == ATTACK) {
    if (0 < slot->eg_out && slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
      s = lookup_attack_step(slot, eg_counter);
      slot->eg_out += (~slot->eg_out * s) >> 3;
    }
  } else {
    if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
      slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
    }
  }

  switch (slot->eg_state) {
  case ATTACK:
    if (slot->eg_out == 0) {
      slot->eg_state = DECAY;
      request_update(slot, UPDATE_EG);
    }
    break;

  case DECAY:
    if ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL) {
      slot->eg_state = SUSTAIN;
      request_update(slot, UPDATE_EG);
    }
    break;

  case SUSTAIN:
  case RELEASE:
  default:
    break;
  }

  if (test) {
    slot->eg_out = 0;
  }
}

static void update_slots(OPL *opl) {
  int i;
  opl->eg_counter++;

  for (i = 0; i < 18; i++) {
    OPL_SLOT *slot = &opl->slot[i];
    if (slot->update_requests) {
      commit_slot_update(slot, opl->notesel);
    }
    calc_envelope(slot, opl->eg_counter, opl->test_flag & 1);
    calc_phase(slot, opl->pm_phase, opl->pm_mode, opl->test_flag & 4);
  }
}

/* input: 0..8191 output: -4095..4095 */
static INLINE int16_t lookup_exp_table(uint16_t i) {
  /* from andete's expressoin */
  int16_t t = (exp_table[(i & 0xff) ^ 0xff] + 1024);
  int16_t res = t >> ((i & 0x7f00) >> 8);
  return ((i & 0x8000) ? ~res : res) << 1;
}

static INLINE int16_t to_linear(uint16_t h, OPL_SLOT *slot, int16_t am) {
  uint16_t att;
  if (slot->eg_out >= EG_MAX)
    return 0;

  att = min(EG_MUTE, (slot->eg_out + slot->tll + am)) << 3;
  return lookup_exp_table(h + att);
}

static INLINE int16_t calc_slot_car(OPL *opl, int ch, int16_t fm) {
  OPL_SLOT *slot = CAR(opl, ch);

  uint8_t am = slot->patch->AM ? opl->lfo_am : 0;

  slot->output[1] = slot->output[0];
  slot->output[0] = to_linear(slot->wave_table[(slot->pg_out + 2 * (fm >> 1)) & (PG_WIDTH - 1)], slot, am);

  return slot->output[0];
}

static INLINE int16_t calc_slot_mod(OPL *opl, int ch) {
  OPL_SLOT *slot = MOD(opl, ch);

  int16_t fm = slot->patch->FB > 0 ? (slot->output[1] + slot->output[0]) >> (9 - slot->patch->FB) : 0;
  uint8_t am = slot->patch->AM ? opl->lfo_am : 0;

  slot->output[1] = slot->output[0];
  slot->output[0] = to_linear(slot->wave_table[(slot->pg_out + fm) & (PG_WIDTH - 1)], slot, am);

  return slot->output[0];
}

static INLINE int16_t calc_slot_tom(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_TOM]);

  return to_linear(slot->wave_table[slot->pg_out], slot, 0);
}

/* Specify phase offset directly based on 10-bit (1024-length) sine table */
#define _PD(phase) ((PG_BITS < 10) ? (phase >> (10 - PG_BITS)) : (phase << (PG_BITS - 10)))

static INLINE int16_t calc_slot_snare(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_SD]);

  uint32_t phase;

  if (BIT(opl->slot[SLOT_HH].pg_out, PG_BITS - 2))
    phase = (opl->noise & 1) ? _PD(0x300) : _PD(0x200);
  else
    phase = (opl->noise & 1) ? _PD(0x0) : _PD(0x100);

  return to_linear(slot->wave_table[phase], slot, 0);
}

static INLINE int16_t calc_slot_cym(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_CYM]);

  uint32_t phase = opl->short_noise ? _PD(0x300) : _PD(0x100);

  return to_linear(slot->wave_table[phase], slot, 0);
}

static INLINE int16_t calc_slot_hat(OPL *opl) {
  OPL_SLOT *slot = &(opl->slot[SLOT_HH]);

  uint32_t phase;

  if (opl->short_noise)
    phase = (opl->noise & 1) ? _PD(0x2d0) : _PD(0x234);
  else
    phase = (opl->noise & 1) ? _PD(0x34) : _PD(0xd0);

  return to_linear(slot->wave_table[phase], slot, 0);
}

#define _MO(x) (-(x) >> 1)
#define _RO(x) (x)

static INLINE int16_t calc_fm(OPL *opl, int ch) {
  if (opl->ch_alg[ch]) {
    return calc_slot_car(opl, ch, 0) + calc_slot_mod(opl, ch);
  }
  return calc_slot_car(opl, ch, calc_slot_mod(opl, ch));
}

static void latch_timer1(OPL *opl) { opl->timer1_counter = opl->reg[0x02] << 2; }

static void latch_timer2(OPL *opl) { opl->timer2_counter = opl->reg[0x03] << 4; }

static void csm_key_on(OPL *opl) {
  opl->csm_key_count = 1;
  update_key_status(opl);
}

static void csm_key_off(OPL *opl) {
  opl->csm_key_count = 0;
  update_key_status(opl);
}

static void update_timer(OPL *opl) {
  if (opl->csm_mode && 0 < opl->csm_key_count) {
    csm_key_off(opl);
  }

  if (opl->reg[0x04] & 0x01) {
    opl->timer1_counter++;
    if (opl->timer1_counter >> 10) {
      opl->status |= 0x40; // timer1 overflow
      if (opl->csm_mode) {
        csm_key_on(opl);
      }
      if (opl->timer1_func) {
        opl->timer1_func(opl->timer1_user_data);
      }
      latch_timer1(opl);
    }
  }

  if (opl->reg[0x04] & 0x02) {
    opl->timer2_counter++;
    if (opl->timer2_counter >> 12) {
      opl->status |= 0x20; // timer2 overflow
      if (opl->timer2_func) {
        opl->timer2_func(opl->timer2_user_data);
      }
      latch_timer2(opl);
    }
  }
}

static void update_output(OPL *opl) {
  int16_t *out;
  int i;

  update_timer(opl);
  update_ampm(opl);
  update_short_noise(opl);
  update_slots(opl);

  out = opl->ch_out;

  /* CH1-6 */
  for (i = 0; i < 6; i++) {
    if (!(opl->mask & OPL_MASK_CH(i))) {
      out[i] = _MO(calc_fm(opl, i));
    }
  }

  /* CH7 */
  if (!opl->rhythm_mode) {
    if (!(opl->mask & OPL_MASK_CH(6))) {
      out[6] = _MO(calc_fm(opl, 6));
    }
  } else {
    if (!(opl->mask & OPL_MASK_BD)) {
      out[9] = _RO(calc_fm(opl, 6));
    }
  }
  update_noise(opl, 14);

  /* CH8 */
  if (!opl->rhythm_mode) {
    if (!(opl->mask & OPL_MASK_CH(7))) {
      out[7] = _MO(calc_fm(opl, 7));
    }
  } else {
    if (!(opl->mask & OPL_MASK_HH)) {
      out[10] = _RO(calc_slot_hat(opl));
    }
    if (!(opl->mask & OPL_MASK_SD)) {
      out[11] = _RO(calc_slot_snare(opl));
    }
  }
  update_noise(opl, 2);

  /* CH9 */
  if (!opl->rhythm_mode) {
    if (!(opl->mask & OPL_MASK_CH(8))) {
      out[8] = _MO(calc_fm(opl, 8));
    }
  } else {
    if (!(opl->mask & OPL_MASK_TOM)) {
      out[12] = _RO(calc_slot_tom(opl));
    }
    if (!(opl->mask & OPL_MASK_CYM)) {
      out[13] = _RO(calc_slot_cym(opl));
    }
  }
  update_noise(opl, 2);

  /* ADPCM */
  if (opl->adpcm != NULL && !(opl->mask & OPL_MASK_ADPCM)) {
    out[14] = OPL_ADPCM_calc(opl->adpcm);
  }
}

INLINE static void mix_output(OPL *opl) {
  int16_t out = 0;
  int i;
  for (i = 0; i < 15; i++) {
    out += opl->ch_out[i];
  }
  if (opl->conv) {
    OPL_RateConv_putData(opl->conv, 0, out);
  } else {
    opl->mix_out[0] = out;
  }
}

INLINE static void mix_output_stereo(OPL *opl) {
  int16_t *out = opl->mix_out;
  int i;
  out[0] = out[1] = 0;
  for (i = 0; i < 15; i++) {
    if (opl->pan[i] & 2)
      out[0] += (int16_t)(opl->ch_out[i] * opl->pan_fine[i][0]);
    if (opl->pan[i] & 1)
      out[1] += (int16_t)(opl->ch_out[i] * opl->pan_fine[i][1]);
  }
  if (opl->conv) {
    OPL_RateConv_putData(opl->conv, 0, out[0]);
    OPL_RateConv_putData(opl->conv, 1, out[1]);
  }
}

/***********************************************************

                   External Interfaces

***********************************************************/

OPL *OPL_new(uint32_t clk, uint32_t rate) {
  OPL *opl;

  if (!table_initialized) {
    initializeTables();
  }

  opl = (OPL *)calloc(sizeof(OPL), 1);
  if (opl == NULL)
    return NULL;

  opl->adpcm = NULL;
  opl->clk = clk;
  opl->rate = rate;
  opl->mask = 0;
  opl->conv = NULL;
  opl->mix_out[0] = 0;
  opl->mix_out[1] = 0;
  opl->timer1_func = NULL;
  opl->timer1_user_data = NULL;
  opl->timer2_func = NULL;
  opl->timer2_user_data = NULL;

  OPL_reset(opl);

  return opl;
}

void OPL_delete(OPL *opl) {
  if (opl->conv) {
    OPL_RateConv_delete(opl->conv);
    opl->conv = NULL;
  }
  if (opl->adpcm) {
    OPL_ADPCM_delete(opl->adpcm);
    opl->adpcm = NULL;
  }
  free(opl);
}

static void reset_rate_conversion_params(OPL *opl) {
  const double f_out = opl->rate;
  const double f_inp = opl->clk / 72;

  opl->out_time = 0;
  opl->out_step = ((uint32_t)f_inp) << 8;
  opl->inp_step = ((uint32_t)f_out) << 8;

  if (opl->conv) {
    OPL_RateConv_delete(opl->conv);
    opl->conv = NULL;
  }

  if (floor(f_inp) != f_out && floor(f_inp + 0.5) != f_out) {
    opl->conv = OPL_RateConv_new(f_inp, f_out, 2);
  }

  if (opl->conv) {
    OPL_RateConv_reset(opl->conv);
  }
}

void refresh_adpcm_object(OPL *opl) {
  if (opl->chip_type == TYPE_Y8950) {
    if (opl->adpcm == NULL) {
      opl->adpcm = OPL_ADPCM_new(opl->clk);
    }
  } else {
    if (opl->adpcm != NULL) {
      free(opl->adpcm);
      opl->adpcm = NULL;
    }
  }
  if (opl->adpcm != NULL) {
    OPL_ADPCM_reset(opl->adpcm);
  }
}

void OPL_reset(OPL *opl) {
  int i;

  if (!opl)
    return;

  opl->adr = 0;

  opl->csm_mode = 0;
  opl->csm_key_count = 0;
  opl->notesel = 0;

  opl->status = 0;
  opl->timer1_counter = 0;
  opl->timer2_counter = 0;

  opl->pm_phase = 0;
  opl->am_phase = 0;

  opl->noise = 1;
  opl->mask = 0;

  opl->rhythm_mode = 0;
  opl->slot_key_status = 0;
  opl->eg_counter = 0;

  reset_rate_conversion_params(opl);

  for (i = 0; i < 18; i++) {
    reset_slot(&opl->slot[i], i);
  }

  for (i = 0; i < 9; i++) {
    opl->ch_alg[i] = 0;
  }

  for (i = 0; i < 0x100; i++) {
    opl->reg[i] = 0;
  }
  opl->reg[0x04] = 0x18; // MASK_EOS | MASK_BUF_RDY

  opl->pm_dphase = PM_DP_WIDTH / (1024 * 8);

  for (i = 0; i < 15; i++) {
    opl->pan[i] = 3;
    opl->pan_fine[i][1] = opl->pan_fine[i][0] = 1.0f;
  }

  for (i = 0; i < 15; i++) {
    opl->ch_out[i] = 0;
  }

  refresh_adpcm_object(opl);
}

void OPL_setRate(OPL *opl, uint32_t rate) {
  opl->rate = rate;
  reset_rate_conversion_params(opl);
}

void OPL_setQuality(OPL *opl, uint8_t q) {}

void OPL_setChipType(OPL *opl, uint8_t type) {
  if (type < TYPE_MAX) {
    opl->chip_type = type;
    refresh_adpcm_object(opl);
  }
}

void OPL_writeIO(OPL *opl, uint32_t adr, uint8_t val) {
  if (adr & 1)
    OPL_writeReg(opl, opl->adr, val);
  else
    opl->adr = val;
}

void OPL_setPan(OPL *opl, uint32_t ch, uint8_t pan) { opl->pan[ch & 15] = pan; }

void OPL_setPanFine(OPL *opl, uint32_t ch, float pan[2]) {
  opl->pan_fine[ch & 15][0] = pan[0];
  opl->pan_fine[ch & 15][1] = pan[1];
}

int16_t OPL_calc(OPL *opl) {
  while (opl->out_step > opl->out_time) {
    opl->out_time += opl->inp_step;
    update_output(opl);
    mix_output(opl);
  }
  opl->out_time -= opl->out_step;
  if (opl->conv) {
    opl->mix_out[0] = OPL_RateConv_getData(opl->conv, 0);
  }
  return opl->mix_out[0];
}

void OPL_calcStereo(OPL *opl, int32_t out[2]) {
  while (opl->out_step > opl->out_time) {
    opl->out_time += opl->inp_step;
    update_output(opl);
    mix_output_stereo(opl);
  }
  opl->out_time -= opl->out_step;
  if (opl->conv) {
    out[0] = OPL_RateConv_getData(opl->conv, 0);
    out[1] = OPL_RateConv_getData(opl->conv, 1);
  } else {
    out[0] = opl->mix_out[0];
    out[1] = opl->mix_out[1];
  }
}

uint32_t OPL_setMask(OPL *opl, uint32_t mask) {
  uint32_t ret;

  if (opl) {
    ret = opl->mask;
    opl->mask = mask;
    return ret;
  } else
    return 0;
}

uint32_t OPL_toggleMask(OPL *opl, uint32_t mask) {
  uint32_t ret;

  if (opl) {
    ret = opl->mask;
    opl->mask ^= mask;
    return ret;
  } else
    return 0;
}

void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t data) {

  int32_t s, c;

  static int32_t stbl[32] = {0,  2,  4,  1,  3,  5,  -1, -1, 6,  8,  10, 7,  9,  11, -1, -1,
                             12, 14, 16, 13, 15, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  reg = reg & 0xff;

  if ((reg == 0x04) && (data & 0x80)) {
    // IRQ RESET
    opl->status = 0;
    opl->reg[0x04] &= 0x7f;
    if (opl->adpcm) {
      OPL_ADPCM_resetStatus(opl->adpcm);
    }
    return;
  }

  opl->reg[reg] = data;

  if (reg == 0x01) {

    opl->test_flag = data;

  } else if (reg == 0x04) {

    if (data & 0x01) {
      latch_timer1(opl);
    }
    if (data & 0x02) {
      latch_timer2(opl);
    }

  } else if (0x07 <= reg && reg <= 0x12) {

    if (reg == 0x08) {
      opl->csm_mode = (data >> 7) & 1;
      opl->notesel = (data >> 6) & 1;
    }

    if (opl->adpcm != NULL && opl->chip_type == TYPE_Y8950) {
      OPL_ADPCM_writeReg(opl->adpcm, reg, data);
    }

  } else if (0x20 <= reg && reg < 0x40) {

    s = stbl[reg - 0x20];
    if (s >= 0) {
      opl->slot[s].patch->AM = (data >> 7) & 1;
      opl->slot[s].patch->PM = (data >> 6) & 1;
      opl->slot[s].patch->EG = (data >> 5) & 1;
      opl->slot[s].patch->KR = (data >> 4) & 1;
      opl->slot[s].patch->ML = (data) & 15;
      request_update(&(opl->slot[s]), UPDATE_ALL);
    }

  } else if (0x40 <= reg && reg < 0x60) {

    s = stbl[reg - 0x40];
    if (s >= 0) {
      opl->slot[s].patch->KL = (data >> 6) & 3;
      opl->slot[s].patch->TL = (data) & 63;
      request_update(&(opl->slot[s]), UPDATE_ALL);
    }

  } else if (0x60 <= reg && reg < 0x80) {

    s = stbl[reg - 0x60];
    if (s >= 0) {
      opl->slot[s].patch->AR = (data >> 4) & 15;
      opl->slot[s].patch->DR = (data) & 15;
      request_update(&(opl->slot[s]), UPDATE_EG);
    }

  } else if (0x80 <= reg && reg < 0xa0) {

    s = stbl[reg - 0x80];
    if (s >= 0) {
      opl->slot[s].patch->SL = (data >> 4) & 15;
      opl->slot[s].patch->RR = (data) & 15;
      request_update(&(opl->slot[s]), UPDATE_EG);
    }

  } else if (0xa0 <= reg && reg < 0xa9) {

    c = reg - 0xa0;
    set_fnumber(opl, c, data + ((opl->reg[reg + 0x10] & 3) << 8));

  } else if (0xb0 <= reg && reg < 0xb9) {

    c = reg - 0xb0;
    set_fnumber(opl, c, ((data & 3) << 8) + opl->reg[reg - 0x10]);
    set_block(opl, c, (data >> 2) & 7);
    update_key_status(opl);

  } else if (0xc0 <= reg && reg < 0xc9) {

    c = reg - 0xc0;
    opl->slot[c * 2].patch->FB = (data >> 1) & 7;
    opl->ch_alg[c] = data & 1;

  } else if (reg == 0xbd) {

    update_rhythm_mode(opl);
    update_key_status(opl);
    opl->am_mode = (data >> 7) & 1;
    opl->pm_mode = (data >> 6) & 1;

  } else if (0xe0 <= reg && reg < 0x100) {
    if (opl->chip_type == TYPE_YM3812 && (opl->reg[0x01] & 0x20)) {
      s = stbl[reg - 0xe0];
      if (s >= 0) {
        opl->slot[s].patch->WS = data & 3;
        request_update(&(opl->slot[s]), UPDATE_WS);
      }
    }
  }
}

uint8_t OPL_readIO(OPL *opl) { return opl->reg[opl->adr]; }

uint8_t OPL_status(OPL *opl) {
  uint8_t status = opl->status;

  if (opl->adpcm) {
    status |= OPL_ADPCM_status(opl->adpcm);
  }

  status &= ~(opl->reg[0x04] & 0x78); // IRQ MASK

  if (status & 0x78) {
    return status | 0x80; // IRQ=1
  }
  return status & 0x7f; // IRQ = 0
}

void OPL_writeADPCMData(OPL *opl, uint8_t type, uint32_t start, uint32_t length, const uint8_t *data) {
  if (opl->adpcm != NULL) {
    if (type == 0) {
      OPL_ADPCM_writeRAM(opl->adpcm, start, length, data);
    } else {
      OPL_ADPCM_writeROM(opl->adpcm, start, length, data);
    }
  }
}