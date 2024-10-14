/*
 * Copyright (c) 2024 Visenri.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Functions to change RP2040 SDK memcpy rom function pointer.
 */
#include "memops_opt.h"
#include <pico/bootrom.h>

#ifndef ROM_TABLE_CODE
    #define ROM_TABLE_CODE(c1, c2) ((c1) | ((c2) << 8))
#endif
#ifndef ROM_FUNC_MEMCPY
    #define ROM_FUNC_MEMCPY ROM_TABLE_CODE('M', 'C')
#endif

#define AEABI_MEM_FUNCS_COUNT 4
// Array of function pointers where memcpy pointer is stored by SDK:
extern void * aeabi_mem_funcs[AEABI_MEM_FUNCS_COUNT];

// Function to replace memcpy pointer in the SDK array.
// It replaces the pointer with the provided function and stores the array position in a static variable for next calls.
// If called with "NULL", it replaces the pointer with the default optimized function: "memcpy_armv6m".
void memcpy_wrapper_replace(void * (*function)(void *, const void *, size_t))
{
    static int8_t posInFunctionArray = -1;

    if (function == NULL) // By default replace it by the optimized version.
        function = &memcpy_armv6m;

    if (posInFunctionArray < 0) // Array position is unknown, execute search.
    {
        void * fn = rom_func_lookup(ROM_FUNC_MEMCPY); // Get pointer to ROM memcpy
        for (int8_t i = 0; i < AEABI_MEM_FUNCS_COUNT; i++)
        {
            if (aeabi_mem_funcs[i] == fn)
            {
                aeabi_mem_funcs[i] = (void*) function;
                posInFunctionArray = i;
                break;
            }
        }
    }
    else // Array position is known, just replace the function pointer in the array.
        aeabi_mem_funcs[posInFunctionArray] = (void*) function;
}

// Function to restore the original ROM function for memcpy.
void memcpy_wrapper_set_to_rom(void)
{
    memcpy_wrapper_replace((void * (*)(void *, const void *, size_t))rom_func_lookup(ROM_FUNC_MEMCPY));
}