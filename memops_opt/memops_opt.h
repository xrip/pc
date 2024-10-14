#ifndef MEMOPS_OPT_H
#define MEMOPS_OPT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void * memcpy_armv6m(void *dst, const void *src, size_t length);

void memcpy_wrapper_replace(void * (*function)(void *, const void *, size_t));
void memcpy_wrapper_set_to_rom(void);

#ifdef __cplusplus
}
#endif
#endif
