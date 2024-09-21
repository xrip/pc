# CMakeLists.txt for usbfs - part of the PicoW C/C++ Boilerplate Project
#
# This library uses FatFS (http://elm-chan.org/fsw/ff/00index_e.html) to provide
# a mountable USB filesystem to Pico C/C++ SDK based projects.
#
# Although developed for the PicoW C++ Boilerplate, this can be easily dropped
# into your own projects - see the documentation for more details.
#
# Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
# This file is released under the BSD 3-Clause License; see LICENSE for details.

add_library(usbfs
        # First, the source for the FatFS library.
        ${CMAKE_CURRENT_LIST_DIR}/diskio.c
        ${CMAKE_CURRENT_LIST_DIR}/ff.c
        ${CMAKE_CURRENT_LIST_DIR}/ffunicode.c

        # Next, the interfaces for TinyUSB
        ${CMAKE_CURRENT_LIST_DIR}/storage.c
        ${CMAKE_CURRENT_LIST_DIR}/usb.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c

        # And lastly, the user-facing routines
        ${CMAKE_CURRENT_LIST_DIR}/usbfs.c
)

# Make our header file(s) accessible both within and external to the library.
target_include_directories(usbfs PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# Specify the Pico C/C++ SDK libraries we need
target_link_libraries(usbfs
        pico_stdlib pico_unique_id
        hardware_flash tinyusb_device
)