/*
 * usbfs/storage.cpp - part of the PicoW C/C++ Boilerplate Project
 *
 * These functions provide a common storage backend, to be used both by TinyUSB
 * and FatFS. The storage is simply a small section of flash at the end of memory.
 * 
 * We allocated 128 sectors (~512kb) of storage, which the minimum that FatFS
 * will allow us to use. This takes quite a chunk out of the Pico flash, but it
 * is what it is.
 *
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * This file is released under the BSD 3-Clause License; see LICENSE for details.
 */

/* System headers. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"


/* Local headers. */

#include "usbfs.h"


/* Module variables. */

static const uint32_t m_storage_size = FLASH_SECTOR_SIZE * 128;
static const uint32_t m_storage_offset = PICO_FLASH_SIZE_BYTES - m_storage_size;


/* Functions.*/

/*
 * get_size - provides size information about storage. 
 */

void storage_get_size( uint16_t *p_block_size, uint32_t *p_num_blocks )
{
  /* Very simple look up. */
  *p_block_size = FLASH_SECTOR_SIZE;
  *p_num_blocks = m_storage_size / FLASH_SECTOR_SIZE;

  /* All done. */
  return;
}


/*
 * read - fetches data from flash.
 */

int32_t storage_read( uint32_t p_sector, uint32_t p_offset, 
                      void *p_buffer, uint32_t p_size_bytes )
{
  /* Very simple copy out of flash then! */
  memcpy( 
    p_buffer, 
    (uint8_t *)XIP_NOCACHE_NOALLOC_BASE + m_storage_offset + p_sector * FLASH_SECTOR_SIZE + p_offset, 
    p_size_bytes
  );

  /* And return how much we read. */
  return p_size_bytes;
}


/*
 * write - stores data into flash, with appropriate guards. 
 */

int32_t storage_write( uint32_t p_sector, uint32_t p_offset,
                       const uint8_t *p_buffer, uint32_t p_size_bytes )
{
  uint32_t l_status;

  /* Don't want to be interrupted. */
  l_status = save_and_disable_interrupts();

  /* Erasing with an offset of 0? Seems odd, but... */
  if ( p_offset == 0 )
  {
    flash_range_erase( m_storage_offset + p_sector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE );
  }

  /* And just write the data now. */
  flash_range_program( 
    m_storage_offset + p_sector * FLASH_SECTOR_SIZE + p_offset, 
    p_buffer, p_size_bytes
  );

  /* Lastly, restore our interrupts. */
  restore_interrupts( l_status );

  /* Before returning the amount of data written. */
  return p_size_bytes;
}


/* End of file usbfs/storage.cpp */
