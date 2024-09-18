#pragma once

#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif
#define VIDEORAM_SIZE (256 << 10)
#define RAM_SIZE (640 * 1024)
#define SOUND_FREQUENCY 44100
#define rgb(r, g, b) ((r<<16) | (g << 8 ) | b )

extern uint8_t log_debug;

extern uint8_t VIDEORAM[VIDEORAM_SIZE];
extern uint8_t RAM[RAM_SIZE];
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

extern void doirq(uint8_t irqnum);
extern uint8_t in8259(uint16_t portnum);
extern void out8259(uint16_t portnum, uint8_t value);
extern uint8_t nextintr();

// Video
extern int videomode;
#define CURSOR_X RAM[0x450]
#define CURSOR_Y RAM[0x451]
extern uint8_t cursor_start, cursor_end;
extern uint32_t vga_palette[256];
extern uint32_t tga_palette[16];
extern void videoram_write(uint32_t address, uint8_t value);
extern uint8_t videoram_read(uint32_t address);

// CGA
extern uint32_t cga_palette[16];
extern const uint8_t cga_gfxpal[3][2][4];
extern uint32_t cga_composite_palette[3][16];
extern uint8_t cga_intensity, cga_colorset, cga_foreground_color, cga_blinking;
extern uint16_t cga_portin(uint16_t portnum);
extern void cga_portout(uint16_t portnum, uint16_t value);

// EGA/VGA
#define vga_plane_size (32768)
extern uint32_t vga_plane_offset;
extern uint8_t vga_planar_mode;
extern void vga_portout(uint16_t portnum, uint16_t value);
extern uint16_t vga_portin(uint16_t portnum);
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
extern uint8_t portram[0x400];
extern uint8_t port60, port61, port64, port3DA;
extern uint32_t vram_offset;
// CPU
extern void exec86(uint32_t execloops);
extern void reset86();

// Disks
extern void diskhandler();
extern uint8_t insertdisk(uint8_t drivenum, const char *pathname);

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

extern int timer_period;
extern uint8_t speakerenabled;
extern uint8_t in8253(uint16_t portnum);
extern void out8253(uint16_t portnum, uint8_t value);

// Mouse
extern void mouse_portout(uint16_t portnum, uint8_t value);
extern uint8_t mouse_portin(uint16_t portnum);
extern void sermouseevent(uint8_t buttons, int8_t xrel, int8_t yrel);


extern void tandy_write(uint16_t reg, uint8_t value);
extern void adlib_write(uint16_t reg, uint8_t value);
extern void cms_write(uint16_t reg, uint8_t value);
#ifdef __cplusplus
}
#endif

