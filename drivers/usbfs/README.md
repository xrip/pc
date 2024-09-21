USBFS
=====

This is a library for providing an easy-to-use USB mounted drive to a Pico
based application. The 'drive' is accessible via `fopen`-like functions, as
well as from the host machine.

It is designed to provide a relatively easy way to provide configuration
details to an application (such as WiFi details) that would otherwise require
recompilation and redeployment.

The library is written in pure C, so can be safely used with C or C++ based
projects.


Usage
-----

The library is baked into the [Picow C/C++ Boilerplate Project](https://github.com/ahnlak/picow-boilerplate),
however it's possible to simply copy the entire `usbfs` directory into your
own project, and include it in your `CMakeList.txt` file, and then simply
`#include "usbfs.h"`:

```
add_subdirectory(usbfs)
target_link_libraries(${NAME} 
    usbfs
)
```

(where `${NAME}` is the filename of your project).


Functions
---------

`usbfs_init()` should be called at your program's startup. It initialises the
USB interface (via TinyUSB), as well as the filesystem.

`usbfs_update()` should be called regularly, in your main loop. USB processing
is polled rather than run in the background, so this ensures that any required
work is performed in a timely manner.

`usbfs_sleep_ms()` is a drop-in replacement for `sleep_ms()`, which will call
`usbfs_update()` for you on a regular basis during your sleep. If you are using
this to manage the timing of your main loop, there is no need to call `usbfs_update()`
directly.

`usbfs_open()`, `usbfs_close()`, `usbfs_read()`, `usbfs_write()`, `usbfs_gets()`
and `usbfs_puts()` are essentially the equivalents of the standard C equivalents,
to allow you to interract with files stored on this filesystem.

`usbfs_timestamp()` returns the modification date/time of the named file, to make
it easy to detect changes.


Caveats
-------

Due to the limitations of FAT (related to sector size and count), this library
requires the use of the top 512kb of flash memory. This is probably worth
keeping in mind if you have a large application.
