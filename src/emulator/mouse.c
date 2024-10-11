#include "emulator.h"

struct sermouse_s {
    uint8_t reg[8];
    uint8_t buf[16];
    int8_t bufptr;
    int baseport;
} sermouse;


static inline void bufsermousedata(uint8_t value) {
    if (sermouse.bufptr == 16)
        return;
    if (sermouse.bufptr == 0)
        doirq(4);
    sermouse.buf[sermouse.bufptr++] = value;
}

 void mouse_portout(uint16_t portnum, uint8_t value) {
    uint8_t oldreg;
    // printf("[DEBUG] Serial mouse, port %X out: %02X\n", portnum, value);
    portnum &= 7;
    oldreg = sermouse.reg[portnum];
    sermouse.reg[portnum] = value;
    switch (portnum) {
        case 4: //modem control register
            if ((value & 1) != (oldreg & 1)) {
                //software toggling of this register
                sermouse.bufptr = 0; //causes the mouse to reset and fill the buffer
                bufsermousedata('M'); //with a bunch of ASCII 'M' characters.
                bufsermousedata('M'); //this is intended to be a way for
                bufsermousedata('M'); //drivers to verify that there is
                bufsermousedata('M'); //actually a mouse connected to the port.
                bufsermousedata('M');
            }
            break;
    }
}

 uint8_t mouse_portin(uint16_t portnum) {
    uint8_t temp;
    // printf("[DEBUG] Serial mouse, port %X in\n", portnum);
    portnum &= 7;
    switch (portnum) {
        case 0: //data receive
            temp = sermouse.buf[0];
            memmove(sermouse.buf, &sermouse.buf[1], 15);
            sermouse.bufptr--;
            if (sermouse.bufptr < 0)
                sermouse.bufptr = 0;
            if (sermouse.bufptr > 0)
                doirq(4);
            sermouse.reg[4] = ~sermouse.reg[4] & 1;
            return temp;
        case 5: //line status register (read-only)
            if (sermouse.bufptr > 0)
                temp = 1;
            else
                temp = 0;
            return 0x1;
        /*default:
            return 0x60 | temp;*/
    }
    return sermouse.reg[portnum & 7];
}

void sermouseevent(uint8_t buttons, int8_t xrel, int8_t yrel) {
    uint8_t highbits = (xrel < 0) ? 3 : 0;
    if (yrel < 0)
        highbits |= 12;
    bufsermousedata(0x40 | (buttons << 4) | highbits);
    bufsermousedata(xrel & 63);
    bufsermousedata(yrel & 63);
}
