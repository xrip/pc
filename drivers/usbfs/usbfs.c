/*
 * usbfs/usbfs.cpp - part of the PicoW C/C++ Boilerplate Project
 *
 * usbfs is the library that handles presenting a filesystem to the host
 * over USB; the main aim is to make it easy to present configuration files
 * to remove the need to recompile (for example, WiFi settings)
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
#include "ff.h"
#include "diskio.h"
#include "usbfs.h"


/* Module variables. */

static FATFS m_fatfs;


/* Functions.*/

/*
 * init - initialised the TinyUSB library, and also the FatFS handling.
 */

void usbfs_init( void )
{
  FRESULT   l_result;
  MKFS_PARM l_options;

  /* First order of the day, is TinyUSB. */
  tusb_init();

  /* And also mount our FatFS partition. */
  l_result = f_mount( &m_fatfs, "", 1 );

  /* If there was no filesystem, make one. */
  if ( l_result == FR_NO_FILESYSTEM )
  {
    /* Set up the options, and format. */
    l_options.fmt = FM_ANY | FM_SFD;
    l_result = f_mkfs( "", &l_options, m_fatfs.win, FF_MAX_SS );
    if ( l_result != FR_OK )
    {
      return;
    }

    /* Set the label on the volume to something sensible. */
    f_setlabel( UFS_LABEL );

    /* And re-mount. */
    l_result = f_mount( &m_fatfs, "", 1 );
  }

  /* All done. */
  return;
}


/*
 * update - run any updates required on TinyUSB or the filesystem.
 */

void usbfs_update( void )
{
  /* Ask TinyUSB to run any outstanding tasks. */
  tud_task();

  /* All done. */
  return;
}


/*
 * sleep_ms - a replacement for the standard sleep_ms function; this will
 *            run update in a busy loop until we reach the requested time,
 *            to ensure that TinyUSB doesn't run into trouble.
 */

void usbfs_sleep_ms( uint32_t p_milliseconds )
{
  absolute_time_t l_target_time;

  /* Work out when we want to 'sleep' until. */
  l_target_time = make_timeout_time_ms( p_milliseconds );

  /* Now enter a busy(ish) loop until that time. */
  while( !time_reached( l_target_time ) )
  {
    /* Run any updates. */
    tud_task();
  }

  /* All done. */
  return;
}


/*
 * open - opens a file in the FatFS filesystem. Takes the same filename and mode
 *        strings as fopen()
 */

usbfs_file_t *usbfs_open( const char *p_pathname, const char *p_mode )
{
  BYTE          l_mode;
  usbfs_file_t *l_fptr;
  FRESULT       l_result;

  /* We need to translate the fopen-style mode into FatFS style bits. */
  if ( strcmp( p_mode, "r" ) == 0 )
  {
    l_mode =  FA_READ;
  }
  else if ( strcmp( p_mode, "r+" ) == 0 )
  {
    l_mode =  FA_READ|FA_WRITE;
  }
  else if ( strcmp( p_mode, "w" ) == 0 )
  {
    l_mode =  FA_CREATE_ALWAYS|FA_WRITE;
  }
  else if ( strcmp( p_mode, "w+" ) == 0 )
  {
    l_mode =  FA_CREATE_ALWAYS|FA_WRITE|FA_READ;
  }
  else if ( strcmp( p_mode, "a" ) == 0 )
  {
    l_mode =  FA_OPEN_APPEND|FA_WRITE;
  }
  else if ( strcmp( p_mode, "a+" ) == 0 )
  {
    l_mode =  FA_OPEN_APPEND|FA_WRITE|FA_READ;
  }
  else
  {
    /* If it's not a mode we support, fail. */
    return NULL;
  }

  /* We'll need a new file structure so save this all in. */
  l_fptr = (usbfs_file_t *)malloc( sizeof( usbfs_file_t ) );
  if ( l_fptr == NULL )
  {
    return NULL;
  }
  memset( l_fptr, 0, sizeof( usbfs_file_t ) );

  /* Good, we know the mode so we can just open the file regularly. */
  l_result = f_open( &l_fptr->fatfs_fptr, p_pathname, l_mode );
  if ( l_result != FR_OK )
  {
    free( l_fptr );
    return NULL;
  }

  /* Make sure our status flags are set right, and return our filepointer. */
  l_fptr->modified = false;
  return l_fptr;
}


/*
 * close - closes a file in the FatFS filesystem; if the file has been modified,
 *         it also informs the USB host that this is so.
 */

bool usbfs_close( usbfs_file_t *p_fileptr )
{
  /* Sanity check the pointer. */
  if ( p_fileptr == NULL )
  {
    return false;
  }

  /* Then simply close the file. */
  f_close( &p_fileptr->fatfs_fptr );

  /* If the file was flagged as modified, let the host know to re-load data. */
  if ( p_fileptr->modified )
  {
    usb_set_fs_changed();
  }

  /* And lastly, free up the memory allocated for our filepointer. */
  free( p_fileptr );

  /* All done. */
  return true;
}


/*
 * read - reads data from an open file, taking similar arguments and providing
 *        the same returns as the standard 'fread()' function; the exception is
 *        that only 'size' is required, no 'nmemb' parameter (because let's face
 *        it, in 95% of cases one or other of those is set to 1 anyway).
 */

size_t usbfs_read( void *p_buffer, size_t p_size, usbfs_file_t *p_fileptr )
{
  UINT      l_bytecount;
  FRESULT   l_result;

  /* Sanity check our parameters. */
  if ( ( p_buffer == NULL ) || ( p_fileptr == NULL ) )
  {
    /* If we don't have valid pointers, we can't read data. */
    return 0;
  }

  /* Then we just send it to FatFS. */
  l_result = f_read( &p_fileptr->fatfs_fptr, p_buffer, p_size, &l_bytecount );
  if ( l_result != FR_OK )
  {
    /* The write has failed. */
    return 0;
  }

  /* Simply return the number of bytes read then. */
  return l_bytecount;
}


/*
 * write - writes data from an open file, taking similar arguments and providing
 *         the same returns as the standard 'fwrite()' function; the exception is
 *         that only 'size' is required, no 'nmemb' parameter (because let's face
 *         it, in 95% of cases one or other of those is set to 1 anyway).
 */

size_t usbfs_write( const void *p_buffer, size_t p_size, usbfs_file_t *p_fileptr )
{
  UINT      l_bytecount;
  FRESULT   l_result;

  /* Sanity check our parameters. */
  if ( ( p_buffer == NULL ) || ( p_fileptr == NULL ) )
  {
    /* If we don't have valid pointers, we can't write data. */
    return 0;
  }

  /* Then we just send it to FatFS. */
  l_result = f_write( &p_fileptr->fatfs_fptr, p_buffer, p_size, &l_bytecount );
  if ( l_result != FR_OK )
  {
    /* The write has failed. */
    return 0;
  }

  /* Flag that we've written data to this file. */
  p_fileptr->modified = true;

  /* Simply return the number of bytes written then. */
  return l_bytecount;
}


/*
 * gets - reads a line of text from the file; takes the same arguments and 
 *        returns the same as the standard 'fgets()' function.
 */

char *usbfs_gets( char *p_buffer, size_t p_size, usbfs_file_t *p_fileptr )
{
  /* Sanity check our parameters. */
  if ( ( p_buffer == NULL ) || ( p_fileptr == NULL ) )
  {
    /* If we don't have valid pointers, we can't read data. */
    return NULL;
  }

  /* Excellent; so, ask FatFS to do the work. */
  return f_gets( p_buffer, p_size, &p_fileptr->fatfs_fptr );
}


/*
 * puts - writes a line of text from the file; takes the same arguments and 
 *        returns the same as the standard 'fputs()' function.
 */

size_t usbfs_puts( const char *p_buffer, usbfs_file_t *p_fileptr )
{
  int l_bytecount;

  /* Sanity check our parameters. */
  if ( ( p_buffer == NULL ) || ( p_fileptr == NULL ) )
  {
    /* If we don't have valid pointers, we can't write data. */
    return -1;
  }

  /* Excellent; so, ask FatFS to do the work. */
  l_bytecount = f_puts( p_buffer, &p_fileptr->fatfs_fptr );

  /* 
   * If a negative value was returned, this indicates an error - otherwise,
   * flag that the file has been modified.
   */
  if ( l_bytecount >= 0 )
  {
    p_fileptr->modified = true;
  }
  return l_bytecount;
}


/*
 * timestamp - fetches the timestamp of the named file; a zero is returned
 *             if the file does not exist. This is encoded as per FatFS, but
 *             can be used without decoding to compare to previous stamps.
 */

uint32_t usbfs_timestamp( const char *p_pathname )
{
  FILINFO   l_fileinfo;
  FRESULT   l_result;

  /* Force a re-mount to make sure timestamps are sync'd */
  f_mount( &m_fatfs, "", 1 );

  /* Ask for information about the file. */
  l_result = f_stat( p_pathname, &l_fileinfo );
  if ( l_result != FR_OK )
  {
    /* If we encountered an error, return a zero timestamp. */
    return 0;
  }

  /* The date and time are both encoded in WORDS; stick them together. */
  return ( l_fileinfo.fdate << 16 ) | l_fileinfo.ftime;
}


/* End of file usbfs/usbfs.cpp */
