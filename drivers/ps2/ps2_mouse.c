#include "ps2_mouse.h"
#include <pico/stdlib.h>
#include <stdbool.h>
#include "string.h"
#include "emulator/emulator.h"

extern int printf_(const char* format, ...);

#define INTELLI_MOUSE 3
#define SCALING_1_TO_1 0xE6
#define RESOLUTION_8_COUNTS_PER_MM 3

enum Commands {
    SET_RESOLUTION = 0xE8,
    REQUEST_DATA = 0xEB,
    SET_REMOTE_MODE = 0xF0,
    GET_DEVICE_ID = 0xF2,
    SET_SAMPLE_RATE = 0xF3,
    RESET = 0xFF,
};

static volatile int bitcount;

static int mouse_initialized = 0;

typedef union {
    struct {
        uint8_t status;
        uint8_t axis_x;
        uint8_t axis_y;
        int8_t wheel;
        uint8_t dummy;
    };
    uint8_t data[5];
} mouse_data_s;

static inline void ps2poll(uint8_t data) {
    static mouse_data_s mouse_data = { 0 };
    static uint8_t packet_size = 0;
    if (!mouse_initialized) {
        printf_("%02x", data);
        return;
    }
    mouse_data.data[packet_size] = data;
    if (++packet_size == 4) {
        static int8_t prev_x = 0, prev_y = 0;
        int8_t xsign = mouse_data.status >> 4 & 1 ? 0 : -1;
        int8_t ysign = mouse_data.status >> 5 & 1 ? 0 : -1;
        //printf_("Data packet %02x %d %d %d\n", mouse_data.status, mouse_data.axis_x, mouse_data.axis_y, mouse_data.wheel);
        sermouseevent( mouse_data.status & 0b111, mouse_data.axis_x - prev_x, -1 * (mouse_data.axis_y - prev_y));
        prev_x = xsign * mouse_data.axis_x;
        prev_y = ysign * mouse_data.axis_y;
        packet_size = 0;
    }

}

static inline void clock_lo(void) {
    gpio_set_dir(MOUSE_CLOCK_PIN, GPIO_OUT);
    gpio_put(MOUSE_CLOCK_PIN, 0);
}

static inline void clock_hi(void) {
    gpio_set_dir(MOUSE_CLOCK_PIN, GPIO_OUT);
    gpio_put(MOUSE_CLOCK_PIN, 1);
}

static inline bool clock_in(void) {
    gpio_set_dir(MOUSE_CLOCK_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(MOUSE_CLOCK_PIN);
}

static inline void data_lo(void) {
    gpio_set_dir(MOUSE_DATA_PIN, GPIO_OUT);
    gpio_put(MOUSE_DATA_PIN, 0);
}

static inline void data_hi(void) {
    gpio_set_dir(MOUSE_DATA_PIN, GPIO_OUT);
    gpio_put(MOUSE_DATA_PIN, 1);
}

static inline bool data_in(void) {
    gpio_set_dir(MOUSE_DATA_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(MOUSE_DATA_PIN);
}

static inline void inhibit(void) {
    clock_lo();
    data_hi();
}

static inline void idle(void) {
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

#define WAIT(stat, us, err) wait_##stat(us)

static inline void int_on(void) {
    gpio_set_dir(MOUSE_CLOCK_PIN, GPIO_IN);
    gpio_set_dir(MOUSE_DATA_PIN, GPIO_IN);
    gpio_set_irq_enabled(MOUSE_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true);
}

static inline void int_off(void) {
    gpio_set_irq_enabled(MOUSE_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, false);
}

static inline int16_t ps2_recv_response(void) {
    busy_wait_ms(25);
    return 0;
}

int16_t mouse_send(uint8_t data) {
    bool parity = true;

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

    //ringbuf_reset(&rbuf);   // clear buffer
    idle();
    int_on();
    return ps2_recv_response();
    ERROR:
//    printf("KBD error %02X \r\n", ps2_error);
//    ps2_error = 0;
    idle();
    int_on();
    return -0xf;
}

void DataHandler(void) {
    static uint8_t incoming = 0;
    static uint32_t prev_ms = 0;
    uint32_t now_ms;
    uint8_t n, val;

    val = gpio_get(MOUSE_DATA_PIN);
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
//        if (ps2bufsize < MOUSE_BUFFER_SIZE) {
//            ps2buffer[ps2bufsize++] = incoming;
            ps2poll(incoming);
//        }
        bitcount = 0;
        incoming = 0;
    }
}

void mouse_init(void) {
    bitcount = 0;

    gpio_init(MOUSE_CLOCK_PIN);
    gpio_init(MOUSE_DATA_PIN);
    gpio_disable_pulls(MOUSE_CLOCK_PIN);
    gpio_disable_pulls(MOUSE_DATA_PIN);
    gpio_set_drive_strength(MOUSE_CLOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(MOUSE_DATA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_dir(MOUSE_CLOCK_PIN, GPIO_IN);
    gpio_set_dir(MOUSE_DATA_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(MOUSE_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true, (gpio_irq_callback_t)&DataHandler);

    mouse_send(RESET);
    busy_wait_ms(100);

    mouse_send(SET_RESOLUTION);
    busy_wait_ms(100);
    mouse_send(RESOLUTION_8_COUNTS_PER_MM);
    busy_wait_ms(100);

    mouse_send(SCALING_1_TO_1 );
    busy_wait_ms(100);


    mouse_send(SET_SAMPLE_RATE);
    mouse_send(200);

    mouse_send(SET_SAMPLE_RATE);
    mouse_send(100);

    mouse_send(SET_SAMPLE_RATE);
    mouse_send(80);

    mouse_send(GET_DEVICE_ID);

    mouse_send(SET_SAMPLE_RATE);
    mouse_send(40);

/*    mouse_send(SET_REMOTE_MODE);
    busy_wait_ms(100);*/

    mouse_send(0xF4); // enable_data_reporting
    busy_wait_ms(25);

    mouse_initialized = 1;
}
