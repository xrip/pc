/*
 * usbfs/usb.cpp - part of the PicoW C/C++ Boilerplate Project
 *
 * These functions are mostly the callbacks required by TinyUSB; it is largely
 * culled from DaftFreak's awesome work on the PicoSystem.
 *
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * This file is released under the BSD 3-Clause License; see LICENSE for details.
 */


/* System headers. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Local headers. */

#include "tusb.h"
#include "usbfs.h"


/* Module variables. */

static bool     m_mounted = true;
static bool     m_fs_changed = false;


/* Functions.*/

/*
 * usb_set_fs_changed - sets the flag to indicate that a file has been locally
 *                      changed; in turn, this ensures that the host is told
 *                      that a re-scan is required.
 */

void usb_set_fs_changed( void )
{
  /* Very simple flag set. */
  m_fs_changed = true;
  return;
}


/*
 * tud_mount_cb - Invoked when device is mounted (configured)
 */

void tud_mount_cb( void )
{
  /* All we really do is remember that we're mounted. */
  m_mounted = true;
  return;
}


/*
 * tud_umount_cb - Invoked when device is unmounted
 */

void tud_umount_cb( void )
{
  /* As before, just remember this. */
  m_mounted = false;
  return;
}


/* Mass Storage (MSC) callbacks. */

/*
 * tud_msc_read10_cb - Invoked when received SCSI READ10 command
 *
 * Fill the buffer (up to bufsize) with address contents and return number of 
 * read bytes.
 *
 * If read < bufsize, these bytes are transferred first and callback invoked
 * again for remaining data.
 *
 * If read == 0, it indicates we're not ready yet e.g disk I/O busy. Callback
 * will be invoked again with the same parameters later on.
 *
 * If read < 0, indicates an error e.g invalid address. This request will be
 * STALLed and return failed status in command status wrapper phase.
 */

int32_t tud_msc_read10_cb( uint8_t p_lun, uint32_t p_lba,
                           uint32_t p_offset, void *p_buffer, 
                           uint32_t p_bufsize )
{
  /* We simply pass on this request to the storage layer. */
  return storage_read( p_lba, p_offset, p_buffer, p_bufsize );
}


/*
 * tud_msc_write10_cd - Invoked when received SCSI WRITE10 command
 *
 * Write data from buffer to address (up to bufsize) and return number of
 * written bytes.
 *
 * If write < bufsize, callback invoked again with remaining data later on.
 *
 * If write == 0, it indicates we're not ready yet e.g disk I/O busy.
 * Callback will be invoked again with the same parameters later on.
 *
 * If write < 0, it indicates an error e.g invalid address. This request will
 * be STALLed and return failed status in command status wrapper phase.
 */

int32_t tud_msc_write10_cb( uint8_t p_lun, uint32_t p_lba,
                            uint32_t p_offset, uint8_t *p_buffer, 
                            uint32_t p_bufsize )
{
  /* We simply pass on this request to the storage layer. */
  return storage_write( p_lba, p_offset, p_buffer, p_bufsize );
}


/*
 * tud_msc_inquiry_cb - Invoked when received SCSI_CMD_INQUIRY
 *
 * Return the vendor, product and revision strings in the provider buffers.
 */

void tud_msc_inquiry_cb( uint8_t p_lun, uint8_t p_vendor_id[8], 
                         uint8_t p_product_id[16], uint8_t p_product_rev[4] )
{
  const char *l_vid = USB_VENDOR_STR;
  const char *l_pid = USB_PRODUCT_STR " Storage";
  const char *l_rev = "1.0";

  memcpy( p_vendor_id  , l_vid, strlen(l_vid) );
  memcpy( p_product_id , l_pid, strlen(l_pid) );
  memcpy( p_product_rev, l_rev, strlen(l_rev) );
}


/*
 * tud_msc_test_unit_ready_cb - Invoked when received Test Unit Ready command.
 *
 * If we have made local changes to files, we raise a SENSE_NOT_READY and return
 * false to indicate that the host needs to re-scan. Otherwise, true indicates
 * that all is well.
 */

bool tud_msc_test_unit_ready_cb( uint8_t p_lun )
{
  /* If files have changed locally, or if we're not mounted, we're not ready. */
  if( m_fs_changed || !m_mounted )
  {
    tud_msc_set_sense( p_lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00 );
    m_fs_changed = false;
    return false;
  }

  /* Otherwise, we are. */
  return true;
}


/*
 * tud_msc_capacity_cb - Invoked when received SCSI_CMD_READ_CAPACITY_10 and
 * SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size.
 *
 * Simply return information about our block size and counts.
 */

void tud_msc_capacity_cb( uint8_t p_lun, 
                          uint32_t *p_block_count, uint16_t *p_block_size )
{
  /* This is a question for the storage layer. */
  storage_get_size( p_block_size, p_block_count );
  return;
}


/*
 * tud_msc_scsi_cb - Invoked when receivind a SCSI command not handled by its
 * own callback.
 *
 * Returns the actual bytes processed, can be zero for no-data command.
 * A negative return indicates error e.g unsupported command, tinyusb will STALL
 * the corresponding endpoint and return failed status in command status wrapper phase.
 */

int32_t tud_msc_scsi_cb( uint8_t p_lun, uint8_t const p_scsi_cmd[16],
                         void *p_buffer, uint16_t p_bufsize )
{
  int32_t  l_retval = 0;

  /* Decide what we're doing based on the SCSI command. */
  switch ( p_scsi_cmd[0] )
  {
    /* This is fine. */
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      l_retval = 0;
      break;

    /* By default, send an ILLEGAL_REQUEST back, and fail. */
    default:
      tud_msc_set_sense( p_lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00 );
      l_retval = -1;
      break;
  }

  /* Return the appopriate code. */
  return l_retval;
}


/*
 * tud_msc_start_stop_cb - Invoked when received Start Stop Unit command
 *
 * start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
 * start = 1 : active mode, if load_eject = 1 : load disk storage
 */

bool tud_msc_start_stop_cb( uint8_t p_lun, uint8_t p_power_condition,
                            bool p_start, bool p_load_eject )
{
  /* If the load_eject flag isn't set, we don't need to do anything. */
  if( p_load_eject )
  {
    /* If not starting, we're unloading our drive - flag it as unmounted. */
    if ( !p_start )
    {
      m_mounted = false;
    }
  }

  /* All is well. */
  return true;
}


/*
 * tud_msc_is_writable_cb - Invoked to check if device is writable
 */

bool tud_msc_is_writable_cb( uint8_t p_lun )
{
  /* We're always writable. */
  return true;
}


/* Communications Device (CDC) callbacks. */

/* We don't implement any of these, leaving them to the Pico stdio handling. */


/* End of file usbfs/usb.cpp */
