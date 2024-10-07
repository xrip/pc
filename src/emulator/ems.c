// Copyright (c) 2019-2024 Andreas T Jonsson <mail@andreasjonsson.se>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software in
//    a product, an acknowledgment (see the following) in the product
//    documentation is required.
//
//    This product make use of the VirtualXT software emulator.
//    Visit https://virtualxt.org for more information.
//
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.

// Reference: https://www.lo-tech.co.uk/wiki/Lo-tech_2MB_EMS_Board

#include "emulator/emulator.h"

// The Lo-tech EMS board driver is hardcoded to 2MB.
#define EMS_MEMORY_SIZE 0x200000

static uint8_t EMS_MEMORY[EMS_MEMORY_SIZE];
static uint8_t ems_pages[4];

void out_ems(uint16_t port, uint8_t data) {
    ems_pages[port & 3] = data;
}

inline static uint32_t physical_address(uint32_t address) {
    uint32_t page_addr = address & 0x3FFF;
    uint8_t selector = ems_pages[(address >> 14) & 3];
    return selector * 0x4000 + page_addr;
}

uint8_t ems_read(uint32_t addr) {
    uint32_t phys_addr = physical_address(addr);
    return (phys_addr < EMS_MEMORY_SIZE) ? EMS_MEMORY[phys_addr] : 0xFF;
}

// TODO: Overlap?
uint16_t ems_readw(uint32_t addr) {
    uint32_t phys_addr = physical_address(addr);
    return (phys_addr < EMS_MEMORY_SIZE) ? *(uint16_t *) &EMS_MEMORY[phys_addr] : 0xFFFF;
}

void ems_write(uint32_t addr, uint8_t data) {
    uint32_t phys_addr = physical_address(addr);
    if (phys_addr < EMS_MEMORY_SIZE)
        EMS_MEMORY[phys_addr] = data;
}


void ems_writew(uint32_t addr, uint16_t data) {
    uint32_t phys_addr = physical_address(addr);
    if (1 + phys_addr < EMS_MEMORY_SIZE)
        *(uint16_t *) &EMS_MEMORY[phys_addr] = data;
}
