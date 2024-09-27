#pragma once

#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef PICO_ON_DEVICE
#define VIDEORAM_SIZE (64 << 10)
#define RAM_SIZE (416 << 10)
#else
#define VIDEORAM_SIZE (128 << 10)
#define RAM_SIZE (640 << 10)
#endif
#define SOUND_FREQUENCY 44100
#define rgb(r, g, b) ((r<<16) | (g << 8 ) | b )

extern uint8_t log_debug;

extern uint8_t VIDEORAM[VIDEORAM_SIZE+2];
extern uint8_t RAM[RAM_SIZE+2];
extern union _bytewordregs_ {
    uint16_t wordregs[8];
    uint8_t byteregs[8];
} regs;
extern uint8_t cf, pf, af, zf, sf, tf, ifl, df, of;
extern uint16_t segregs[4];
// i8259
extern struct i8259_s {
    uint8_t imr; //mask register
    uint8_t irr; //request register
    uint8_t isr; //service register
    uint8_t icwstep; //used during initialization to keep track of which ICW we're at
    uint8_t icw[5];
    uint8_t intoffset; //interrupt vector offset
    uint8_t priority; //which IRQ has highest priority
    uint8_t autoeoi; //automatic EOI mode
    uint8_t readmode; //remember what to return on read register from OCW3
    uint8_t enabled;
} i8259;

#define doirq(irqnum) i8259.irr |= (1 << irqnum)

static inline uint8_t nextintr() {
    uint8_t tmpirr = i8259.irr & (~i8259.imr); //XOR request register with inverted mask register
    for (uint8_t i = 0; i < 8; i++)
        if ((tmpirr >> i) & 1) {
            i8259.irr ^= (1 << i);
            i8259.isr |= (1 << i);
            return (i8259.icw[2] + i);
        }
}

void out8259(uint16_t portnum, uint8_t value);

uint8_t in8259(uint16_t portnum);
// Video
extern int videomode;
#define CURSOR_X RAM[0x450]
#define CURSOR_Y RAM[0x451]
extern uint8_t cursor_start, cursor_end;
extern uint32_t vga_palette[256];

// TGA
extern uint32_t tga_palette[16];
extern void tga_portout(uint16_t portnum, uint16_t value);


// CGA
extern const uint32_t cga_palette[16];
extern const uint8_t cga_gfxpal[3][2][4];
extern uint32_t cga_composite_palette[3][16];
extern uint8_t cga_intensity, cga_colorset, cga_foreground_color, cga_blinking, cga_hires;

void cga_portout(uint16_t portnum, uint16_t value);

uint16_t cga_portin(uint16_t portnum);

// EGA/VGA
#define vga_plane_size (16000)
extern uint32_t vga_plane_offset;
extern uint8_t vga_planar_mode;

void vga_portout(uint16_t portnum, uint16_t value);

uint16_t vga_portin(uint16_t portnum);

// Memory
extern void writew86(uint32_t addr32, uint16_t value);

extern void write86(uint32_t addr32, uint8_t value);

extern uint16_t readw86(uint32_t addr32);

extern uint8_t read86(uint32_t addr32);

extern void portout(uint16_t portnum, uint16_t value);

extern void portout16(uint16_t portnum, uint16_t value);

extern uint16_t portin(uint16_t portnum);

extern uint16_t portin16(uint16_t portnum);

// Ports
extern uint8_t port60, port61, port64, port3DA;
extern uint32_t vram_offset;
extern uint32_t tga_offset;
// CPU
extern void exec86(uint32_t execloops);
extern void reset86();

// i8253
extern struct i8253_s {
    uint16_t chandata[3];
    uint8_t accessmode[3];
    uint8_t bytetoggle[3];
    uint32_t effectivedata[3];
    float chanfreq[3];
    uint8_t active[3];
    uint16_t counter[3];
} i8253;

void out8253(uint16_t portnum, uint8_t value);

uint8_t in8253(uint16_t portnum);

extern int timer_period;
extern int speakerenabled;

// Mouse
void sermouseevent(uint8_t buttons, int8_t xrel, int8_t yrel);

uint8_t mouse_portin(uint16_t portnum);

void mouse_portout(uint16_t portnum, uint8_t value);

extern void tandy_write(uint16_t reg, uint8_t value);

extern void adlib_write_d(uint16_t reg, uint8_t value);
extern void cms_write(uint16_t reg, uint8_t value);

extern void dss_out(uint16_t portnum, uint8_t value);

extern uint8_t dss_in(uint16_t portnum);

extern void covox_out(uint16_t portnum, uint8_t value);

extern uint8_t dss_sample();

extern int16_t speaker_sample();

extern void sn76489_reset();

extern void sn76489_out(uint16_t value);

extern int16_t sn76489_sample();

extern void cms_out(uint16_t portnum, uint16_t value);

extern uint8_t cms_in(uint16_t addr);

extern void cms_samples(int *output);;

#define XMS_DRIVER 1
#define XMS_FN_CS 0x0000
#define XMS_FN_IP 0x03FF

extern uint8_t xms_handler();
#ifdef __cplusplus
}
#endif

