#pragma once
#ifndef PICO_USB_DRIVE
#define PICO_USB_DRIVE

#include "ff.h"
#include "diskio.h"

void init_pico_usb_drive();
void pico_usb_drive_heartbeat();

// msc_disk.c
bool tud_msc_ejected();
void set_tud_msc_ejected(bool v);

enum {
  DISK_BLOCK_SIZE = 512,
  FAT_OFFSET = 0x1000
};
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);

#endif
