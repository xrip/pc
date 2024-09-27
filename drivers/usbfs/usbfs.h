/*
 * usbfs/usbfs.h - part of the PicoW C/C++ Boilerplate Project
 *
 * usbfs is the library that handles presenting a filesystem to the host
 * over USB; the main aim is to make it easy to present configuration files
 * to remove the need to recompile (for example, WiFi settings)
 * 
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * This file is released under the BSD 3-Clause License; see LICENSE for details.
 */

#pragma once

#include "ff.h"


/* Constants. */

#define USB_VENDOR_ID       0xCafe
#define USB_VENDOR_STR      "USBFS"
#define USB_PRODUCT_STR     "PicoW"

#define UFS_LABEL           "PicoW"


/* Structures */

typedef struct
{
  FIL   fatfs_fptr;
  bool  modified;
} usbfs_file_t;


/* Function prototypes. */

/* Internal functions. */

void            storage_get_size( uint16_t *, uint32_t * );
int32_t         storage_read( uint32_t, uint32_t, void *, uint32_t );
int32_t         storage_write( uint32_t, uint32_t, const uint8_t *, uint32_t );

void            usb_set_fs_changed( void );

/* Public functions. */

#ifdef __cplusplus
extern "C" {
#endif

void            usbfs_init( void );
void            usbfs_update( void );
void            usbfs_sleep_ms( uint32_t );

usbfs_file_t   *usbfs_open( const char *, const char * );
bool            usbfs_close( usbfs_file_t * );
size_t          usbfs_read( void *, size_t, usbfs_file_t * );
size_t          usbfs_write( const void *, size_t, usbfs_file_t * );
char           *usbfs_gets( char *, size_t, usbfs_file_t * );
size_t          usbfs_puts( const char *, usbfs_file_t * );
uint32_t        usbfs_timestamp( const char * );

#ifdef __cplusplus
}
#endif


/* End of file usbfs/usbfs.h */
