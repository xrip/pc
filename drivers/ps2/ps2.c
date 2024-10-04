#include "ps2.h"
#include <pico/stdlib.h>
#include <stdbool.h>
#include "string.h"
#include "hardware/irq.h"


volatile int bitcount;
static uint8_t ps2bufsize = 0;
uint8_t ps2buffer[KBD_BUFFER_SIZE];
uint8_t kbloop = 0;

uint8_t led_status = 0b000;

#define PS2_ERR_NONE    0

volatile int16_t ps2_error = PS2_ERR_NONE;

void ps2poll();

static void clock_lo(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_OUT);
    gpio_put(KBD_CLOCK_PIN, 0);
}

static inline void clock_hi(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_OUT);
    gpio_put(KBD_CLOCK_PIN, 1);
}

static bool clock_in(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(KBD_CLOCK_PIN);
}

static void data_lo(void) {
    gpio_set_dir(KBD_DATA_PIN, GPIO_OUT);
    gpio_put(KBD_DATA_PIN, 0);
}

static void data_hi(void) {
    gpio_set_dir(KBD_DATA_PIN, GPIO_OUT);
    gpio_put(KBD_DATA_PIN, 1);
}

static inline bool data_in(void) {
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(KBD_DATA_PIN);
}

static void inhibit(void) {
    clock_lo();
    data_hi();
}

static void idle(void) {
    clock_hi();
    data_hi();
}

#define wait_us(us)     busy_wait_us_32(us)
#define wait_ms(ms)     busy_wait_ms(ms)

static inline uint16_t wait_clock_lo(uint16_t us) {
    while (clock_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

static inline uint16_t wait_clock_hi(uint16_t us) {
    while (!clock_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

static inline uint16_t wait_data_lo(uint16_t us) {
    while (data_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

static inline uint16_t wait_data_hi(uint16_t us) {
    while (!data_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

#define WAIT(stat, us, err) do { \
    if (!wait_##stat(us)) { \
        ps2_error = err; \
        goto ERROR; \
    } \
} while (0)

static void int_on(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);
    gpio_set_irq_enabled(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true);
}

static void int_off(void) {
    gpio_set_irq_enabled(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, false);
}

static int16_t ps2_recv_response(void) {
    // Command may take 25ms/20ms at most([5]p.46, [3]p.21)
    uint8_t retry = 25;
    int16_t c = -1;
    while (retry-- && (c = ps2buffer[ps2bufsize]) == -1) {
        wait_ms(1);
    }
    return c;
}

int16_t keyboard_send(uint8_t data) {
    bool parity = true;
    ps2_error = PS2_ERR_NONE;

    //printf("KBD set s%02X \r\n", data);

    int_off();

    /* terminate a transmission if we have */
    inhibit();
    wait_us(200);

    /* 'Request to Send' and Start bit */
    data_lo();
    wait_us(200);
    clock_hi();
    WAIT(clock_lo, 15000, 1); // 10ms [5]p.50

    /* Data bit[2-9] */
    for (uint8_t i = 0; i < 8; i++) {
        wait_us(15);
        if (data & (1 << i)) {
            parity = !parity;
            data_hi();
        }
        else {
            data_lo();
        }
        WAIT(clock_hi, 100, (int16_t) (2 + i * 0x10));
        WAIT(clock_lo, 100, (int16_t) (3 + i * 0x10));
    }

    /* Parity bit */
    wait_us(15);
    if (parity) { data_hi(); }
    else { data_lo(); }
    WAIT(clock_hi, 100, 4);
    WAIT(clock_lo, 100, 5);

    /* Stop bit */
    wait_us(15);
    data_hi();

    /* Ack */
    WAIT(data_lo, 100, 6); // check Ack
    WAIT(data_hi, 100, 7);
    WAIT(clock_hi, 100, 8);

    memset(ps2buffer, 0x00, sizeof ps2buffer);
    //ringbuf_reset(&rbuf);   // clear buffer
    idle();
    int_on();
    return ps2_recv_response();
ERROR:
//    printf("KBD error %02X \r\n", ps2_error);
    ps2_error = 0;
    idle();
    int_on();
    return -0xf;
}

void keyboard_toggle_led(uint8_t led) {
    led_status ^= led;

    keyboard_send(0xED);
    busy_wait_ms(50);
    keyboard_send(led_status);
}

uint8_t ps2_to_xt_1(uint32_t val) {
    uint8_t i;
    for (i = 0; i < 85; i++) {
        if (ps2_group1[i].make == val) return ps2_group1[i].xt_make;
    }
    return 0;
}

uint8_t ps2_to_xt_2(uint32_t val) {
    uint8_t i;
    for (i = 0; i < 16; i++) {
        if (ps2_group2[i].xt_make == val) return ps2_group2[i].make;
    }
    return 0;
}

uint32_t ps2getcode() {
    uint32_t retval, i, len;
    if (!ps2bufsize) return 0;
    switch (ps2buffer[0]) {
        case 0xF0:
        case 0xE0:
        case 0xE1:
            len = 2;
            break;
        default:
            len = 1;
            break;
    }
    if (ps2bufsize < len) return 0;
    if (ps2buffer[0] == 0xE0) {
        if (ps2buffer[1] == 0xF0) len = 3;
    }
    if (ps2bufsize < len) return 0;
    retval = 0;

    //translate code
    if (len == 1) {
        retval = ps2_to_xt_1(ps2buffer[0]);
    }
    if (len == 2) {
        if (ps2buffer[0] == 0xF0) retval = ps2_to_xt_1(ps2buffer[1]) | 0x80;
        if (ps2buffer[0] == 0xE0) retval = ps2_to_xt_2(ps2buffer[1]);
    }
    if (len == 3) {
        if ((ps2buffer[0] == 0xE0) && (ps2buffer[1] == 0xF0)) retval = ps2_to_xt_2(ps2buffer[2]) | 0x80;
    }
    //end translate code

    for (i = len; i < KBD_BUFFER_SIZE; i++) {
        ps2buffer[i - len] = ps2buffer[i];
    }

    ps2bufsize -= len;

    // NUMLOCK

    switch (retval) {
        case 0x45:
            keyboard_toggle_led(PS2_LED_NUM_LOCK);
            break;
        case 0x46:
            keyboard_toggle_led(PS2_LED_SCROLL_LOCK);
            break;
        case 0x3A:
            keyboard_toggle_led(PS2_LED_CAPS_LOCK);
            break;
    }
    return retval;
}

void KeyboardHandler(void) {
    static uint8_t incoming = 0;
    static uint32_t prev_ms = 0;
    uint32_t now_ms;
    uint8_t n, val;

    val = gpio_get(KBD_DATA_PIN);
    now_ms = time_us_64();
    if (now_ms - prev_ms > 250) {
        bitcount = 0;
        incoming = 0;
    }
    prev_ms = now_ms;
    n = bitcount - 1;
    if (n <= 7) {
        incoming |= (val << n);
    }
    bitcount++;
    if (bitcount == 11) {
        if (ps2bufsize < KBD_BUFFER_SIZE) {
            ps2buffer[ps2bufsize++] = incoming;
            ps2poll();
        }
        bitcount = 0;
        incoming = 0;
    }
    kbloop = 1;
}

void keyboard_init(void) {
    bitcount = 0;
    memset(ps2buffer, 0, KBD_BUFFER_SIZE);

    gpio_init(KBD_CLOCK_PIN);
    gpio_init(KBD_DATA_PIN);
    gpio_disable_pulls(KBD_CLOCK_PIN);
    gpio_disable_pulls(KBD_DATA_PIN);
    gpio_set_drive_strength(KBD_CLOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(KBD_DATA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true,
                                       (gpio_irq_callback_t)&KeyboardHandler); //

    // Blink all 3 leds
    keyboard_send(0xFF); //Reset and start self-test
    sleep_ms(400); // Why so long?

    keyboard_send(0xF2); // Get keyvoard id https://wiki.osdev.org/PS/2_Keyboard
    sleep_ms(250);

    /*
    ps2_send(0xED);
    sleep_ms(50);
    ps2_send(2); // NUM

    ps2_send(0xED);
    sleep_ms(50);
    ps2_send(3); // SCROLL
*/
    /*    ps2_send(0xED);
        sleep_ms(50);
        ps2_send(7);*/

    return;
}

extern bool handleScancode(uint32_t ps2scancode);

void ps2poll() {
    uint32_t ps2scancode = ps2getcode();
    if (!ps2scancode) {
        return;
    }

    if (handleScancode(ps2scancode)) {
        return;
    }
#if 0
    portram[0x60] = ps2scancode;
    // char tmp[20]; sprintf(tmp, "sc: 0x%X", ps2scancode); logMsg(tmp);
    portram[0x64] |= 2;
    doirq(1);
#endif
}
