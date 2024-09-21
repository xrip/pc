/*
 * usbfs/diskio.cpp - part of the PicoW C/C++ Boilerplate Project
 *
 * These functions provide callbacks for FatFS to talk to our storage layer.
 * 
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * This file is released under the BSD 3-Clause License; see LICENSE for details.
 */

/* System headers. */

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"

/* Local headers. */

#include "ff.h"
#include "diskio.h"
#include "usbfs.h"


/* Functions.*/


/*
 * disk_initialize - called to initializes the storage device.
 */

DSTATUS disk_initialize( BYTE pdrv )
{
  /* Our storage is always initialized. */
  return RES_OK;
}


/*
 * disk_status - called to inquire the current drive status.
 */

DSTATUS disk_status( BYTE pdrv )
{
  /* Our storage is always available. */
  return RES_OK;
}


/*
 * disk_read - called to read data from the storage device.
 */

DRESULT disk_read( BYTE pdrv, BYTE *buff, LBA_t sector, UINT count )
{
  int32_t l_bytecount;

  /* Make sure that we have a fixed sector size. */
  static_assert( FF_MIN_SS == FF_MAX_SS, "FatFS Sector Size not fixed!" );

  /* Ask the storage layer to perform the read. */
  l_bytecount = storage_read( sector, 0, buff, FF_MIN_SS*count );

  /* Check that we wrote as much data as expected. */
  if ( l_bytecount != FF_MIN_SS*count )
  {
    return RES_ERROR;
  }

  /* All went well then. */
  return RES_OK;
}


/*
 * disk_write - called to write data to the storage device.
 */

DRESULT disk_write( BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count )
{
  int32_t l_bytecount;

  /* Make sure that we have a fixed sector size. */
  static_assert( FF_MIN_SS == FF_MAX_SS, "FatFS Sector Size not fixed!" );

  /* Ask the storage layer to perform the write. */
  l_bytecount = storage_write( sector, 0, buff, FF_MIN_SS*count );

  /* Check that we wrote as much data as expected. */
  if ( l_bytecount != FF_MIN_SS*count )
  {
    return RES_ERROR;
  }

  /* All went well then. */
  return RES_OK;
}


/*
 * disk_ioctl - called to control device specific features and miscellaneous
 *              functions other than generic read/write.
 */

DRESULT disk_ioctl( BYTE pdrv, BYTE cmd, void* buff )
{
  uint16_t block_size;
  uint32_t num_blocks;

  /* Handle each command as required. */
  switch(cmd)
  {
    case CTRL_SYNC: 
      /* We have no cache, so we're always synced. */
      return RES_OK;

    case GET_SECTOR_COUNT:
      /* Just ask the storage layer for this data. */
      storage_get_size( &block_size, &num_blocks );
      *(LBA_t *)buff = num_blocks;
      return RES_OK;

    case GET_BLOCK_SIZE:
      *(DWORD *)buff = 1;
      return RES_OK;
  }

  /* Any other command we receive is an error as we do not handle it. */
  return RES_PARERR;
}

/* End of file usbfs/diskio.cpp */
