#include <stdio.h>
#include "pico/stdlib.h"
//#include "pico/critical_section.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "ringbuf.h"




/*
 * PS/2(IBMPC/AT Code Set 2) keyboard converter
 *
 * License: MIT
 * Copyright 2022 Jun WAKO <wakojun@gmail.com>
 *
 */
#define xprintf(s, ...)         printf(s, ##__VA_ARGS__)

// input pins: 2:clock(IRQ), 3:data
#define CLOCK_PIN   0
#define DATA_PIN    1

#define PS2_ERR_NONE    0

volatile int16_t ps2_error = PS2_ERR_NONE;

#define PS2_LED_SCROLL_LOCK 0
#define PS2_LED_NUM_LOCK    1
#define PS2_LED_CAPS_LOCK   2

volatile int8_t ps2_led = -1;
uint16_t ps2_kbd_id = 0xFFFF;


#define BUF_SIZE 16
static uint8_t buf[BUF_SIZE];
static ringbuf_t rbuf = {
    .buffer = buf,
    .head = 0,
    .tail = 0,
    .size_mask = BUF_SIZE - 1
};

#define wait_us(us)     busy_wait_us_32(us)
#define wait_ms(ms)     busy_wait_ms(ms)
//#define wait_us(us)     sleep_us(us)
//#define wait_ms(ms)     sleep_ms(ms)
#define timer_read32()  board_millis()

void ps2_callback(uint gpio, uint32_t events);
static void ps2_init(void)
{
    ringbuf_reset(&rbuf);
    gpio_init(CLOCK_PIN);
    gpio_init(DATA_PIN);
    gpio_set_pulls(CLOCK_PIN, true, false);
    gpio_set_pulls(DATA_PIN, true, false);
    gpio_set_drive_strength(CLOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(DATA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_dir(DATA_PIN, GPIO_IN);
    gpio_set_dir(CLOCK_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true, &ps2_callback);
}

static void int_on(void)
{
    gpio_set_dir(CLOCK_PIN, GPIO_IN);
    gpio_set_dir(DATA_PIN, GPIO_IN);
    gpio_set_irq_enabled(CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true);
}
static void int_off(void)
{
    gpio_set_irq_enabled(CLOCK_PIN, GPIO_IRQ_EDGE_FALL, false);
}

static void clock_lo(void)
{
    gpio_set_dir(CLOCK_PIN, GPIO_OUT);
    gpio_put(CLOCK_PIN, 0);
}
static inline void clock_hi(void)
{
    gpio_set_dir(CLOCK_PIN, GPIO_OUT);
    gpio_put(CLOCK_PIN, 1);
}
static bool clock_in(void)
{
    gpio_set_dir(CLOCK_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(CLOCK_PIN);
}

static void data_lo(void)
{
    gpio_set_dir(DATA_PIN, GPIO_OUT);
    gpio_put(DATA_PIN, 0);
}
static void data_hi(void)
{
    gpio_set_dir(DATA_PIN, GPIO_OUT);
    gpio_put(DATA_PIN, 1);
}
static inline bool data_in(void)
{
    gpio_set_dir(DATA_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(DATA_PIN);
}

static void inhibit(void)
{
    clock_lo();
    data_hi();
}
static void idle(void)
{
    clock_hi();
    data_hi();
}

static inline uint16_t wait_clock_lo(uint16_t us)
{
    while (clock_in()  && us) { asm(""); wait_us(1); us--; }
    return us;
}
static inline uint16_t wait_clock_hi(uint16_t us)
{
    while (!clock_in() && us) { asm(""); wait_us(1); us--; }
    return us;
}
static inline uint16_t wait_data_lo(uint16_t us)
{
    while (data_in() && us)  { asm(""); wait_us(1); us--; }
    return us;
}
static inline uint16_t wait_data_hi(uint16_t us)
{
    while (!data_in() && us)  { asm(""); wait_us(1); us--; }
    return us;
}

#define WAIT(stat, us, err) do { \
    if (!wait_##stat(us)) { \
        ps2_error = err; \
        goto ERROR; \
    } \
} while (0)

static int16_t ps2_recv(void)
{
    // There are alternative options for ciritcal section protection
    //critical_section_t crit_rbuf;
    //critical_section_init(&crit_rbuf);
    //critical_section_enter_blocking(&crit_rbuf);      // disable IRQ and spin_lock
    //irq_set_enabled(IO_IRQ_BANK0, false);             // disable only GPIO IRQ

    uint32_t status = save_and_disable_interrupts();    // disable IRQ
    int16_t c = ringbuf_get(&rbuf); // critical_section
    restore_interrupts(status);

    //irq_set_enabled(IO_IRQ_BANK0, true);
    //critical_section_exit(&crit_rbuf);

    if (c != -1) printf("r%02X ", c & 0xFF);
    if (ps2_error) { printf("e%02X ", ps2_error); ps2_error = 0; }
    return c;
}

static int16_t ps2_recv_response(void)
{
    // Command may take 25ms/20ms at most([5]p.46, [3]p.21)
    uint8_t retry = 25;
    int16_t c = -1;
    while (retry-- && (c = ps2_recv()) == -1) {
        wait_ms(1);
    }
    return c;
}

int16_t ps2_send(uint8_t data)
{
    bool parity = true;
    ps2_error = PS2_ERR_NONE;

    printf("s%02X ", data);

    int_off();

    /* terminate a transmission if we have */
    inhibit();
    wait_us(200);

    /* 'Request to Send' and Start bit */
    data_lo();
    wait_us(200);
    clock_hi();
    WAIT(clock_lo, 15000, 1);   // 10ms [5]p.50

    /* Data bit[2-9] */
    for (uint8_t i = 0; i < 8; i++) {
        wait_us(15);
        if (data&(1<<i)) {
            parity = !parity;
            data_hi();
        } else {
            data_lo();
        }
        WAIT(clock_hi, 100, (int16_t) (2 + i*0x10));
        WAIT(clock_lo, 100, (int16_t) (3 + i*0x10));
    }

    /* Parity bit */
    wait_us(15);
    if (parity) { data_hi(); } else { data_lo(); }
    WAIT(clock_hi, 100, 4);
    WAIT(clock_lo, 100, 5);

    /* Stop bit */
    wait_us(15);
    data_hi();

    /* Ack */
    WAIT(data_lo, 100, 6);    // check Ack
    WAIT(data_hi, 100, 7);
    WAIT(clock_hi, 100, 8);

    ringbuf_reset(&rbuf);   // clear buffer
    idle();
    int_on();
    return ps2_recv_response();
ERROR:
    printf("e%02X ", ps2_error); ps2_error = 0;
    idle();
    int_on();
    return -0xf;
}

void ps2_callback(uint gpio, uint32_t events) {
    static enum {
        INIT,
        START,
        BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7,
        PARITY,
        STOP,
    } state = INIT;
    static uint8_t data = 0;
    static uint8_t parity = 1;

    // process at falling edge of clock
    if (gpio != CLOCK_PIN) { return; }
    if (events != GPIO_IRQ_EDGE_FALL) { return; }

    state++;
    switch (state) {
        case START:
            // start bit is low
            if (data_in())
                goto ERROR;
            break;
        case BIT0:
        case BIT1:
        case BIT2:
        case BIT3:
        case BIT4:
        case BIT5:
        case BIT6:
        case BIT7:
            data >>= 1;
            if (data_in()) {
                data |= 0x80;
                parity++;
            }
            break;
        case PARITY:
            if (data_in()) {
                if (!(parity & 0x01))
                    goto ERROR;
            } else {
                if (parity & 0x01)
                    goto ERROR;
            }
            break;
        case STOP:
            // stop bit is high
            if (!data_in())
                goto ERROR;
            // critical section for ringbuffer - need to do nothing here
            // because this should be called in IRQ context.
            // Use protection in main thread when using ringuf.
            ringbuf_put(&rbuf, data);
            goto DONE;
            break;
        default:
            goto ERROR;
    }
    return;
ERROR:
    ps2_error = state + 0xF0;
DONE:
    state = INIT;
    data = 0;
    parity = 1;
}

// Code Set 2 -> HID(Usage page << 12 | Usage ID)
// Usage page: 0x0(Keyboard by default), 0x7(Keyboard), 0xC(Consumer), 0x1(Generic Desktiop/System Control)
// https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#code-set-2-to-hid-usage
const uint16_t cs2_to_hid[] = {
    //   0       1       2       3       4       5       6       7       8       9       A       B       C       D       E       F
    0x0000, 0x0042, 0x0000, 0x003E, 0x003C, 0x003A, 0x003B, 0x0045, 0x0068, 0x0043, 0x0041, 0x003F, 0x003D, 0x002B, 0x0035, 0x0067, // 0
    0x0069, 0x00E2, 0x00E1, 0x0088, 0x00E0, 0x0014, 0x001E, 0x0000, 0x006A, 0x0000, 0x001D, 0x0016, 0x0004, 0x001A, 0x001F, 0x0000, // 1
    0x006B, 0x0006, 0x001B, 0x0007, 0x0008, 0x0021, 0x0020, 0x008C, 0x006C, 0x002C, 0x0019, 0x0009, 0x0017, 0x0015, 0x0022, 0x0000, // 2
    0x006D, 0x0011, 0x0005, 0x000B, 0x000A, 0x001C, 0x0023, 0x0000, 0x006E, 0x0000, 0x0010, 0x000D, 0x0018, 0x0024, 0x0025, 0x0000, // 3
    0x006F, 0x0036, 0x000E, 0x000C, 0x0012, 0x0027, 0x0026, 0x0000, 0x0070, 0x0037, 0x0038, 0x000F, 0x0033, 0x0013, 0x002D, 0x0000, // 4
    0x0071, 0x0087, 0x0034, 0x0000, 0x002F, 0x002E, 0x0000, 0x0072, 0x0039, 0x00E5, 0x0028, 0x0030, 0x0000, 0x0031, 0x0000, 0x0073, // 5
    0x0000, 0x0064, 0x0093, 0x0092, 0x008A, 0x0000, 0x002A, 0x008B, 0x0000, 0x0059, 0x0089, 0x005C, 0x005F, 0x0085, 0x0000, 0x0000, // 6
    0x0062, 0x0063, 0x005A, 0x005D, 0x005E, 0x0060, 0x0029, 0x0053, 0x0044, 0x0057, 0x005B, 0x0056, 0x0055, 0x0061, 0x0047, 0x0046, // 7
    0x0000, 0x0000, 0x0000, 0x0040, 0x0046, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 8
    0xC221, 0x00E6, 0x0000, 0x0000, 0x00E4, 0xC0B6, 0x0000, 0x0000, 0xC22A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00E3, // 9
    0xC227, 0xC0EA, 0x0000, 0xC0E2, 0x0000, 0x0000, 0x0000, 0x00E7, 0xC226, 0x0000, 0x0000, 0xC192, 0x0000, 0x0000, 0x0000, 0x0065, // A
    0xC225, 0x0000, 0xC0E9, 0x0000, 0xC0CD, 0x0000, 0x0000, 0x1081, 0xC224, 0x0000, 0xC223, 0xC0B7, 0x0000, 0x0000, 0x0000, 0x1082, // B
    0xC194, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xC18A, 0x0000, 0x0054, 0x0000, 0x0000, 0xC0B5, 0x0000, 0x0000, // C
    0xC183, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0058, 0x0000, 0x0000, 0x0000, 0x1083, 0x0000, // D
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x004D, 0x0000, 0x0050, 0x004A, 0x0000, 0x0000, 0x0000, // E
    0x0049, 0x004C, 0x0051, 0x0000, 0x004F, 0x0052, 0x0000, 0x0048, 0x0000, 0x0000, 0x004E, 0x0000, 0x0046, 0x004B, 0x0048, 0x0000, // F
};

void register_code(uint16_t code, bool make);

// from TMK ibmpc_usb converter
int8_t process_cs2(uint8_t code)
{
    static enum {
        CS2_INIT,
        CS2_F0,
        CS2_E0,
        CS2_E0_F0,
        // Pause
        CS2_E1,
        CS2_E1_14,
        CS2_E1_F0,
        CS2_E1_F0_14,
        CS2_E1_F0_14_F0,
    } state_cs2 = CS2_INIT;

    switch (state_cs2) {
        case CS2_INIT:
            switch (code) {
                case 0xE0:
                    state_cs2 = CS2_E0;
                    break;
                case 0xF0:
                    state_cs2 = CS2_F0;
                    break;
                case 0xE1:
                    state_cs2 = CS2_E1;
                    break;
                case 0x00 ... 0x7F:
                case 0x83:  // F7
                case 0x84:  // Alt'd PrintScreen
                    register_code(cs2_to_hid[code], true);
                    break;
                case 0xF1:  // Korean Hanja          - not support
                case 0xF2:  // Korean Hangul/English - not support
                    break;
                case 0xAA:  // Self-test passed
                case 0xFC:  // Self-test failed
                default:    // unknown codes
                    xprintf("!CS2_INIT!\n");
                    return -1;
            }
            break;
        case CS2_E0:    // E0-Prefixed
            switch (code) {
                case 0x12:  // to be ignored
                case 0x59:  // to be ignored
                    state_cs2 = CS2_INIT;
                    break;
                case 0xF0:
                    state_cs2 = CS2_E0_F0;
                    break;
                default:
                    state_cs2 = CS2_INIT;
                    if (code < 0x80) {
                        register_code(cs2_to_hid[code | 0x80], true);
                    } else {
                        xprintf("!CS2_E0!\n");
                        return -1;
                    }
            }
            break;
        case CS2_F0:    // Break code
            switch (code) {
                case 0x00 ... 0x7F:
                case 0x83:  // F7
                case 0x84:  // Alt'd PrintScreen
                    state_cs2 = CS2_INIT;
                    register_code(cs2_to_hid[code], false);
                    break;
                default:
                    state_cs2 = CS2_INIT;
                    xprintf("!CS2_F0! %02X\n", code);
                    return -1;
            }
            break;
        case CS2_E0_F0: // Break code of E0-prefixed
            switch (code) {
                case 0x12:  // to be ignored
                case 0x59:  // to be ignored
                    state_cs2 = CS2_INIT;
                    break;
                default:
                    state_cs2 = CS2_INIT;
                    if (code < 0x80) {
                        register_code(cs2_to_hid[code | 0x80], false);
                    } else {
                        xprintf("!CS2_E0_F0!\n");
                        return -1;
                    }
            }
            break;
        // Pause make: E1 14 77
        case CS2_E1:
            switch (code) {
                case 0x14:
                    state_cs2 = CS2_E1_14;
                    break;
                case 0xF0:
                    state_cs2 = CS2_E1_F0;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        case CS2_E1_14:
            switch (code) {
                case 0x77:
                    register_code(cs2_to_hid[code | 0x80], true);
                    state_cs2 = CS2_INIT;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        // Pause break: E1 F0 14 F0 77
        case CS2_E1_F0:
            switch (code) {
                case 0x14:
                    state_cs2 = CS2_E1_F0_14;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        case CS2_E1_F0_14:
            switch (code) {
                case 0xF0:
                    state_cs2 = CS2_E1_F0_14_F0;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        case CS2_E1_F0_14_F0:
            switch (code) {
                case 0x77:
                    register_code(cs2_to_hid[code | 0x80], false);
                    state_cs2 = CS2_INIT;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        default:
            state_cs2 = CS2_INIT;
    }
    return 0;
}

void ps2_set_led(int8_t led)
{
    ps2_led = led;

    // keyboard is not ready
    if (ps2_kbd_id == 0xFFFF) return;

    int16_t r;
    r = ps2_send(0xED);
    if (r == 0xFA) {
        wait_us(100);
        r = ps2_send((uint8_t) led);
    }
}

void ps2_task(void)
{
    static uint32_t detect_ms = 0;
    // keyboard detection
    if (ps2_kbd_id == 0xFFFF) {
        if (board_millis() - detect_ms < 1000) return;
        detect_ms = board_millis();

        int16_t r;
        r = ps2_send(0xFF);
        if (r != 0xFA) return;

        wait_ms(500);
        r = ps2_send(0xF2);
        if (r != 0xFA) return;

        wait_ms(500);
        r = ps2_recv();
        ps2_kbd_id = (uint16_t) ((r & 0xFF) << 8);

        wait_ms(500);
        r = ps2_recv();
        ps2_kbd_id = (uint16_t) ((ps2_kbd_id & 0xFF00) | (r & 0x00FF));
        printf("ps2_kbd_id:%04X\n",  ps2_kbd_id);

        if (ps2_led != -1) {
            ps2_set_led(ps2_led);
        }
    }

    // keyboard is not ready
    if (ps2_kbd_id == 0xFFFF) return;

    int16_t c = ps2_recv();
    if (c != -1) {
        // Remote wakeup
        if (tud_suspended()) {
            tud_remote_wakeup();
        }

        int8_t r = process_cs2((uint8_t) c);
        if (r == -1) {
            ps2_kbd_id = 0xFFFF; // reinit keyboard
        }
    }
}






// imported from tinyusb/examples/device/hid_composite
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
void led_blinking_task(void);

int main() {
    board_init();
    tud_init(BOARD_TUD_RHPORT);
    stdio_init_all();

    ps2_init();

    printf("\ntinyusb_ps2\n");
    while (true) {
        ps2_task();
        tud_task();
        led_blinking_task();
    }
    return 0;
}



//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
  //printf("Blink!\n");
}




//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
//
// codes from TMK >>>
//
typedef union {
    uint8_t raw[KEYBOARD_REPORT_SIZE];
    struct {
        uint8_t mods;
        uint8_t reserved;
        uint8_t keys[KEYBOARD_REPORT_KEYS];
    };
//#if defined(NKRO_ENABLE) || defined(NKRO_6KRO_ENABLE)
    struct {
        uint8_t mods;
        uint8_t bits[KEYBOARD_REPORT_BITS];
    } nkro;
//#endif
} __attribute__ ((packed)) report_keyboard_t;
//
// codes from TMK <<<
//

static report_keyboard_t keyboard_report;

void keyboard_add_key(uint8_t key)
{
    if (key >= 0xE0 && key <= 0xE8) {
        keyboard_report.mods |= (uint8_t) (1 << (key & 0x7));
        return;
    }

    // NKRO
    if (tud_hid_n_get_protocol(ITF_NUM_KEYBOARD) == HID_PROTOCOL_REPORT) {
        if ((key >> 3) < KEYBOARD_REPORT_BITS) {
            keyboard_report.nkro.bits[key >> 3] |= (uint8_t) (1 << (key  & 0x7));
        }
        return;
    }

    // 6KRO
    int empty = -1;
    for (int i = 0; i < 6; i++) {
        if (keyboard_report.keys[i] == key) {
            return;
        }
        if (empty == -1 && keyboard_report.keys[i] == 0) {
            empty = i;
        }
    }
    if (empty != -1) {
        keyboard_report.keys[empty] = key;
    }
}

void keyboard_del_key(uint8_t key)
{
    if (key >= 0xE0 && key <= 0xE8) {
        keyboard_report.mods &= (uint8_t) ~(1 << (key & 0x7));
        return;
    }

    // NKRO
    if (tud_hid_n_get_protocol(ITF_NUM_KEYBOARD) == HID_PROTOCOL_REPORT) {
        if ((key >> 3) < KEYBOARD_REPORT_BITS) {
            keyboard_report.nkro.bits[key >> 3] &= (uint8_t) ~(1<<(key & 0x7));
        }
        return;
    }

    // 6KRO
    for (int i = 0; i < 6; i++) {
        if (keyboard_report.keys[i] == key) {
            keyboard_report.keys[i] = 0;
            return;
        }
    }
}

void print_report(void)
{
    printf("\n");
    uint8_t *p = (uint8_t *) &keyboard_report;
    for (uint i = 0; i < sizeof(keyboard_report); i++) {
        printf("%02X ", *p++);
    }
    printf("\n");
}

void register_code(uint16_t code, bool make)
{
    // usage page
    uint8_t page = (uint8_t) ((code & 0xf000) >> 12);
    switch (page) {
        case 0x0:
        case 0x7: // keyboard page
            {
                uint8_t key = (uint8_t) (code & 0xFF);
                if (make) {
                    keyboard_add_key(key);
                } else {
                    keyboard_del_key(key);
                }
                if (tud_hid_n_get_protocol(ITF_NUM_KEYBOARD) == HID_PROTOCOL_BOOT) {
                    tud_hid_n_report(ITF_NUM_KEYBOARD, 0, &keyboard_report, 8);
                } else { // NKRO
                    tud_hid_n_report(ITF_NUM_KEYBOARD, 0, &keyboard_report, sizeof(keyboard_report));
                }
            }
            break;
        case 0xC: // consumer page
            {
                uint16_t usage;
                if (make) {
                    usage = code & 0xFFF;
                } else {
                    usage = 0;
                }
                tud_hid_n_report(ITF_NUM_HID, REPORT_ID_CONSUMER_CONTROL, &usage, sizeof(usage));
            }
            break;
        case 0x1: // system page
            {
                uint16_t usage = code & 0xFFF;
                if (usage != HID_USAGE_DESKTOP_SYSTEM_POWER_DOWN &&
                    usage != HID_USAGE_DESKTOP_SYSTEM_SLEEP &&
                    usage != HID_USAGE_DESKTOP_SYSTEM_WAKE_UP) {
                    return;
                }

                uint8_t report;
                if (make) {
                    report = usage & 0x3;
                } else {
                    report = 0;
                }
                tud_hid_n_report(ITF_NUM_HID, REPORT_ID_SYSTEM_CONTROL, &report, sizeof(report));
            }
            break;
        default:
            break;
    }
    print_report();
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  if (instance != ITF_NUM_KEYBOARD) return;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == 0)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const usb_led = buffer[0];

      printf("LED:%02X ", usb_led);
      int8_t led = 0;
      if (usb_led & KEYBOARD_LED_SCROLLLOCK)
          led |= (1 << PS2_LED_SCROLL_LOCK);
      if (usb_led & KEYBOARD_LED_NUMLOCK)
          led |= (1 << PS2_LED_NUM_LOCK);
      if (usb_led & KEYBOARD_LED_CAPSLOCK)
          led |= (1 << PS2_LED_CAPS_LOCK);
      ps2_set_led(led);

      if (usb_led & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}
