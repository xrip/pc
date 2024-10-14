/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Optimized memcpy for ARM Cortex-M0/M0+ (little endian).
 *
 * Created by Visenri using some parts from:
 *  SDK: bootrom_misc.S     - Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *  newlib: memcpy-armv7m.S - Copyright (c) 2013 ARM Ltd.
 */

// This allows automated testing of all implementation variants.
// Each implementation is given a unique name using this macro
#ifndef MEMCPY_ARMV6M_FUNCTION_NAME
    #define MEMCPY_ARMV6M_FUNCTION_NAME memcpy_armv6m
#endif

.syntax unified
.cpu cortex-m0 // This code is intended for m0 and m0plus (tested using m0plus)
.thumb      // use 16-bit instructions
.section .time_critical.MEMCPY_ARMV6M_FUNCTION_NAME, "ax"
.altmacro

#include "memcpy_armv6m_macros.S"

#ifndef MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS
    #define MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS 2
#endif
#ifndef MEMCPY_ARMV6M_OPTIMIZE_SIZE
    #define MEMCPY_ARMV6M_OPTIMIZE_SIZE 0
#endif
#ifndef MEMCPY_ARMV6M_OPTIMIZE_XIP_MEMORY_READ
    #define MEMCPY_ARMV6M_OPTIMIZE_XIP_MEMORY_READ 1
#endif
#ifndef MEMCPY_ARMV6M_MED_SIZE_SPEED
    #define MEMCPY_ARMV6M_MED_SIZE_SPEED 2
#endif
#ifndef MEMCPY_ARMV6M_MED_SIZE_UPWARDS
    #define MEMCPY_ARMV6M_MED_SIZE_UPWARDS 0
#endif

#define MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_2_WORDS (MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS == 2)
// Conditions to use the "memcpy_copy_src_aligned" macro only for xip memory
#define MEMCPY_ARMV6M_MED_SIZE_ONLY_FOR_XIP_MEMORY ((MEMCPY_ARMV6M_MED_SIZE_SPEED >= 2) && (MEMCPY_ARMV6M_OPTIMIZE_XIP_MEMORY_READ))

// If this macro is 0, MED_SIZE_SPEED is forced to 1, to use the memcpy_copy_src_aligned for all misaligned data sizes
#if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS == 0 
    #undef MEMCPY_ARMV6M_MED_SIZE_SPEED
    #define MEMCPY_ARMV6M_MED_SIZE_SPEED 1
#endif


.align 2
.global MEMCPY_ARMV6M_FUNCTION_NAME
.type MEMCPY_ARMV6M_FUNCTION_NAME, %function
.thumb_func
    nop     // Optional, this shifts function entry point by one halfword, 
            // avoiding an extra nop at next ".align 2" (16 byte copy loop)
            // If we want to keep the function aligned, instead of this nop, another nop can be used, see below
MEMCPY_ARMV6M_FUNCTION_NAME:
    mov   ip, r0
    cmp   r2, #8
    blo   .Lmemcpy_short
    subs  r3, r0, r1
    lsls  r3, #30
    bne   .Lmemcpy_not_aligned

    // r0 and r1 are co-aligned
    push {r4-r6}

    // r3 must be 0 here
    lsls  r4, r0, #30   // Get the 2 lsbs of r0
    beq   .Lmemcpy_word_aligned   // If all 0s, we are word aligned
    lsls  r4, #1        // Bit0 of r0 in r4, Bit1 goes to carry
    beq   2f            // Check the Bit0, eq means Bit0 = 0
    
    // Instructions before the carry check must not affect carry
    // Copy byte at odd address
    ldrb  r4, [r1]
    strb  r4, [r0]
    movs  r3, #1        // Add bytes done (used also as ldrh offset)
    bcs   .Lalign_done  // Check Bit1, if it is 1 we are done, because 11 + 01 = 00
    //nop       // Optional, this avoids an extra nop at next ".align 2" (16 byte copy loop), 
                // So the nop is not always executed, only when 3 bytes are copied.
                // See extra comments at the entry point of this function.
2:
    // Copy halfword on non word boundary
    ldrh  r4, [r1, r3]
    strh  r4, [r0, r3]
    adds  r3, #2        // Add bytes done

.Lalign_done:
    // Adjust pointers and length
    add   r0, r3
    add   r1, r3
    subs  r2, r3

/* ---------------------------------------------------------------
  Aligned copy, src and dst are aligned at word boundary.
  ---------------------------------------------------------------*/
.Lmemcpy_word_aligned:
    subs  r2, #16
    bcc 5f
.align 2 // Make sure the loop is word aligned, so, the 4 instructions are fetched only with 2 RAM reads (32bits each one).
    // 16 byte loop
1:
    ldmia r1!, {r3, r4, r5, r6}
    stmia r0!, {r3, r4, r5, r6}
    subs  r2, #16
    bcs   1b
5:
    // 8 bytes remainder?
    lsls  r2, #29
    bcc   1f
    ldmia r1!, {r3, r4}
    stmia r0!, {r3, r4}
1:
    // 4 bytes remainder?
    lsls  r2, #1
    bcc   1f
    ldmia r1!, {r3}
    stmia r0!, {r3}
1:
    pop   {r4-r6}
    // early out for word aligned ending
    beq   2f
    // 2 bytes remainder?
    lsls  r2, #1
    bcc   1f
    ldrh  r3, [r1]
    strh  r3, [r0]
    beq   2f
    adds  r1, #2
    adds  r0, #2
    // note fall thru into branch that wont take
1:
    // 1 bytes remainder?
    beq   2f
    ldrb  r3, [r1]
.Lmemcpy_word_aligned_store_1:
    strb  r3, [r0]
2:
    mov   r0, ip
    bx    lr

/* ---------------------------------------------------------------
  Special medium size copy for XIP memory, limited by XIP memory speed.
  Minimum copy size: 7.
  Focus on saving size and using ldr/ldmia/ldrh at source (XIP).
  Each new read triggers a new random read sequence (Fast read quad),
  It takes 22 XIP memory cycles to read a byte, but only a couple more to read another byte.
  So it is very important to use ldr/ldmia/ldrh when possible.
  ---------------------------------------------------------------*/
#if MEMCPY_ARMV6M_MED_SIZE_ONLY_FOR_XIP_MEMORY
  #if (MEMCPY_ARMV6M_MED_SIZE_SPEED >= 2) && MEMCPY_ARMV6M_MED_SIZE_UPWARDS && (MEMCPY_ARMV6M_OPTIMIZE_SIZE < 2)
    // Use shorter version, otherwise the ".Lmemcpy_short" at function start gets out of range
    #define MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP 1
    #define MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP_TAIL 1
  #else
    #define MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP MEMCPY_ARMV6M_OPTIMIZE_SIZE
  #endif

.Lmemcpy_med_size:
  #if !MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP
    // With copy tail-return, no size optimization
    memcpy_copy_src_aligned 1, 0 
  #else
    // Without copy tail-return, with size optimization
    memcpy_copy_src_aligned 0, MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP
    #if MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP < 2 && !MEMCPY_ARMV6M_OPTIMIZE_SIZE_XIP_TAIL
    beq   .Lmemcpy_short_end
    ldmia r1!, {r3}
1:
    subs  r2, #1
    beq   .Lmemcpy_word_aligned_store_1
    strb  r3, [r0]
    adds  r0, #1
    lsrs  r3, #8
    b     1b
    #else
    b     .Lmemcpy_short
    #endif
  #endif
#endif //MEMCPY_ARMV6M_MED_SIZE_ONLY_FOR_XIP_MEMORY

#if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS > 0
    // Calculate optimal transition size (based on experimental results).
    #if MEMCPY_ARMV6M_MED_SIZE_SPEED <= 1 && MEMCPY_ARMV6M_OPTIMIZE_SIZE > 0
        #define MEMCPY_ARMV6M_XIP_SIZE_THRESHOLD (64 - (MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS - 1) * 9 - (MEMCPY_ARMV6M_OPTIMIZE_SIZE) * 6)
    #else
        #if MEMCPY_ARMV6M_OPTIMIZE_SIZE >= 2
            #define MEMCPY_ARMV6M_XIP_SIZE_THRESHOLD 20 - (MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS - 1)
        #else
            #define MEMCPY_ARMV6M_XIP_SIZE_THRESHOLD (73 - (MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS - 1) * 13 - (MEMCPY_ARMV6M_OPTIMIZE_SIZE) * 3 - (MEMCPY_ARMV6M_MED_SIZE_UPWARDS) * 15)
        #endif
    #endif

/* ---------------------------------------------------------------
  Copy not aligned, check if size is enough / worth to use misaligned copy.
  ---------------------------------------------------------------*/
.Lmemcpy_not_aligned:
  #if MEMCPY_ARMV6M_OPTIMIZE_XIP_MEMORY_READ && (MEMCPY_ARMV6M_MED_SIZE_SPEED >= 1) \
    && \
    (MEMCPY_ARMV6M_MED_SIZE_ONLY_FOR_XIP_MEMORY || (MEMCPY_ARMV6M_XIP_SIZE_THRESHOLD > 16))
    // Extra checks for XIP memory
    lsrs  r3, r1, #29
    bne   1f // Address >= than 0x20000000
    bcc   1f // Address <  than 0x10000000
    // Address is in XIP memory range: 0x1xxxxxxx
    // Check if it is cached, if it is, it is better to use the code path used for RAM-RAM copy
    #if 1
    lsls  r3, r1, #6
    bcs   1f        // Check bit 26: XIP_SRAM 0x15XXXXXX, XIP_CTRL_BASE 0x14XXXXXX
    lsrs  r3, r3, #30
    beq   1f        // Check bits 25-24: 0x11XXXXXX - 0x13XXXXXX -> no cache, 0x10XXXXXX -> with cache
    #endif
    // A different threshold is used for XIP memory, 
    // for uncached memory, the medium size method gives much faster results, because it uses ldmia / ldrh aligned at src when possible,
    // whereas Lmisaligned_copy uses dst alignment and it may use up to 3 extra ldb instead of the ldmia /ldrh in the worst case.
    //yield
    cmp   r2, MEMCPY_ARMV6M_XIP_SIZE_THRESHOLD //#48 //#24
    bls   .Lmemcpy_med_size
    b     .Lmisaligned_copy
1:
  #endif
    cmp   r2, #16 //14-16 - Threshold for using misaligned copy, see Lmisaligned_copy comments
    bhi   .Lmisaligned_copy
    // Fallthrough to "Lmemcpy_med_size" or "Lmemcpy_short"

/* ---------------------------------------------------------------
  Medium size copy
  ---------------------------------------------------------------*/
  #if MEMCPY_ARMV6M_MED_SIZE_SPEED <= 0
/* ---------------------------------------------------------------
  Low speed, but quite compact version - upwards copy.
  Any size can be copied unsing an 8 byte unrolled loop.
  ---------------------------------------------------------------*/
.Lmemcpy_med_size:
    // Get the offset up to the 8 byte boundary
    movs  r3, #7
    ands  r3, r2
    beq   .Lmemcpy_med_size_loop_start

    // Calculate offset for loop entry point
    negs  r3, r3
    adds  r3, #8    // r3 = 8 - unaligned bytes

    // Offset pointers by the same amount negated, to start at the right address
    subs  r0, r3
    subs  r1, r3

    // Compute a jump setting PC = PC + 4 * entry point
    lsls  r3, #2
    add   pc, r3    // ADD PC and MOV PC branch within Thumb state without interworking

    // This part is skipped in first iteration, because entry point is always after these instructions
.Lmemcpy_med_size_loop:
    // Move src pointer to next 8 byte block
    adds  r1, #8    // Using an instruction here we avoid the need to add a fixed negative offset before the "add pc"
    // Move the data
.Lmemcpy_med_size_loop_start:
    .irp offset,0,1,2,3,4,5,6,7
    ldrb  r3, [r1, #\offset]
    strb  r3, [r0, #\offset]
    .endr

    adds  r0, #8
    subs  r2, #8
    bhi   .Lmemcpy_med_size_loop
    mov   r0, ip
    bx    lr
  #elif (MEMCPY_ARMV6M_MED_SIZE_SPEED >= 2) && MEMCPY_ARMV6M_MED_SIZE_UPWARDS
/* ---------------------------------------------------------------
  High speed, upwards copy.
  Only up to 16 bytes.
  ---------------------------------------------------------------*/
    // Calculate offset for loop entry point
    negs  r3, r2
    adds  r3, #16    // r3 = table size - unaligned bytes

    // Offset pointers by the same amount negated, to start at the right address
    subs  r0, r3
    subs  r1, r3

    // Compute a jump setting PC = PC + 4 * entry point
    lsls  r3, #2
    add   pc, r3    // ADD PC and MOV PC branch within Thumb state without interworking
    nop     // Using an instruction here we avoid the need to add a fixed negative offset before the "add pc"
    .irp offset,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    ldrb  r3, [r1, #\offset]
    strb  r3, [r0, #\offset]
    .endr

    mov   r0, ip
    bx    lr
  #endif
#endif // MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS > 0

#if MEMCPY_ARMV6M_MED_SIZE_SPEED == 1
/* ---------------------------------------------------------------
  Medium speed, upwards copy.
  Minimum copy size: 7.
  Focus on using ldr/ldmia/ldrh at source.
  This results in a good all-round speed/size tradeof.
  It works as fast as possible with XIP memory because of the source alignment.
  ---------------------------------------------------------------*/
  #if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS == 0
  // In this case, this function is used for any not aligned size > 7.
.Lmemcpy_not_aligned:
  #endif
.Lmemcpy_med_size:
  #if !MEMCPY_ARMV6M_OPTIMIZE_SIZE
    memcpy_copy_src_aligned 1, 0    // With copy tail-return, no size optimization
  #else
    // Without copy tail-return.
    // Optimize size, but only up to level 1
    memcpy_copy_src_aligned 0, MEMCPY_ARMV6M_OPTIMIZE_SIZE/2
    // Fall through to Lmemcpy_short to complete the copy
  #endif
#endif

/* ---------------------------------------------------------------
  Small/Medium size copy, downwards.
  Fastest copy for RAM-RAM for small sizes.
  Only up to 7/16 bytes
  ---------------------------------------------------------------*/
.Lmemcpy_short:
    // Compute a jump setting PC = PC + FIXED_OFFSET - 4 * entry point
    // Use "add pc", instead of the classic "adr" followed by "bx rx" or "mov pc":
    // This is shorter than the "bx rx" version and avoids the align requirement.
    // As a bonus, all the math can be done with only 1 register
    lsls  r2, #2
    subs  r2, #1 + (.Lmemcpy_short_end - .Lmemcpy_short_jump_start - 4) // +1 should not be needed, but it doesn't hurt to use it
    negs  r2, r2
    // From ARMv6-M reference manual:
    //  - ADD PC and MOV PC branch within Thumb state without interworking
.Lmemcpy_short_jump_start:
    add   pc, r2

#if (MEMCPY_ARMV6M_MED_SIZE_SPEED >= 2) && !MEMCPY_ARMV6M_MED_SIZE_UPWARDS
    .irp offset,15,14,13,12,11,10,9,8,7
    ldrb  r3, [r1, #\offset]
    strb  r3, [r0, #\offset]
    .endr
#endif
    .irp offset,6,5,4,3,2,1,0
    ldrb  r3, [r1, #\offset]
    strb  r3, [r0, #\offset]
    .endr
.Lmemcpy_short_end:
    mov   r0, ip
    bx    lr

#if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS > 0
/* ---------------------------------------------------------------
  Misaligned word copy (in blocks of 2/1 word => 8/4 bytes)
  ---------------------------------------------------------------*/
.Lmisaligned_copy:
    /* IMPORTANT:
    Data MUST be misaligned, if used with aligned data, code will NOT work correctly!.
    Data size MUST be >=8 (word) or >=12 (2 word) to guarantee code to work correctly.
    For sizes below 12 to 16, misalignment adjustment has more overhead than just byte-to-byte copy.*/

    /* Align dst only, not trying to align src.  That is the because
    handling of aligned src and misaligned dst need more overhead than
    otherwise.  By doing this the worst case is when initial src is aligned,
    up to 4 byte additional copy will executed, which is acceptable. */

    // Push only required registers
  #if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_2_WORDS
    #define PUSHED_REGS r4-r6
  #else
    #define PUSHED_REGS r4-r5
  #endif
    push  {PUSHED_REGS}

    // Check if we have a misaligned dst
    movs  r3, #3
    ands  r3, r0
    beq   .Ldst_aligned

    // We have misaligned dst, calculate size to move
    movs  r5, #4
    subs  r4, r5, r3 // size to move = 4 - misaligned bytes

    // Move the misaligned data
    memcpy_copy_3_2_or_1 r4

    // Move pointers and count
    add   r0, r4
    add   r1, r4
    subs  r2, r4

    /* Now that dst is aligned */
.Ldst_aligned:
    /* dst is aligned, but src isn't.
    Backward r1 by misaligned bytes, to make r1 aligned. */
    movs  r3, #3
    ands  r3, r1
    subs  r1, r3

    /* Since we need to restore r1 to unaligned address after the loop,
    we need keep the offset bytes somewhere and sub it from r1 afterwards.  */
    /* Because this requires an extra register, it is faster to hardcode it in the copy macro,
    saving 2 cycles in push-pop, but using 2 instructions more */

    /* Pre-load first word */
    ldmia r1!, {r4}
    // Decrease count by the 4 / 8 bytes that will be moved inconditionally
    subs  r2, #4 * MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_WORDS

    // Jump to one of the 3 macro calls, depending on the misalignment
    cmp   r3, #2
    beq   .Lmisaligned_copy_2_2 // 2
    bhi   .Lmisaligned_copy_3_1 // 3
                                // 1 Fall through to Lmisaligned_copy_1_3

/* ---------------------------------------------------------------
  Macro to move data in 1 / 2 word blocks with shifting to compensate alignment:
    shift: number of shifts required to align data.
    r0: destination pointer (in-out).
    r1: source pointer (in-out).
    r2: copy size (in-out).
    r3, r4, r5: scratch registers used for the copy.
    r6:         scratch registers used for the copy (only when using 2 words).
    r4: input:  first word preloaded.
    r5: output: misaligned bytes to sub from r1 after the macro, to correct the misalignment.
  ---------------------------------------------------------------*/
    .macro mis_src_copy shift

  #if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_2_WORDS
    // 8 byte copy loop
1:
    lsrs  r5, r4, \shift        // Prepare data to be combined with next word.
    ldmia r1!, {r3, r4}         // Get 2 new words.
    lsls  r6, r3, 32 - \shift   // Shift new data in 1st word.
    orrs  r5, r5, r6            // Combine with previous remaining data.

    lsrs  r3, \shift            // Prepare data to be combined with the 2nd word.
    
    lsls  r6, r4, 32 - \shift   // Shift new data in 2nd word.
    orrs  r6, r3, r6            // Combine with remaining data from 1st word.

    stmia r0!, {r5, r6}         // Store the 2 words.
    subs  r2, #8                // 8 bytes each loop.
    bhs   1b

    // Misalignment Offset = 4 - misaligned bytes, to correct r1 after the copy loop.
    movs  r5, (32 - \shift) / 8
    // Check if there are at least 4 bytes more.
    adds  r2, #4
    bcc   .Lsrc_misaligned_tail_add4

    lsrs  r4, \shift            // Prepare data to be combined with next word.

    // Copy 4 bytes
    ldmia r1!, {r3}             // Read new word.
    lsls  r6, r3, 32 - \shift   // Shift new data.
    orrs  r4, r4, r6            // Combine it with previous remaining data.
    stmia r0!, {r4}             // Store word.
  #else   
    // 4 byte copy loop
1:
    lsrs  r3, r4, \shift        // Prepare data to be combined with next word.
    ldmia r1!, {r4}             // Read new word.
    lsls  r5, r4, 32 - \shift   // Shift new data.
    orrs  r3, r3, r5            // Combine it with previous remaining data.
    stmia r0!, {r3}             // Store word.
    subs  r2, #4                // 4 bytes each loop.
    bhs   1b

    // Misalignment Offset = 4 - misaligned bytes, to correct r1 after the copy loop.
    movs  r5, (32 - \shift) / 8
  #endif
    .endm

    .macro mis_src_jump_to_adjust_size
  #if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_2_WORDS
    #if !MEMCPY_ARMV6M_OPTIMIZE_SIZE
    // Just a jump to Lsrc_misaligned_tail_sub4 could be used.
    // But jumping to Lsrc_misaligned_tail saves 1 cycle
    cmp   r2, #0    // Status based on the value of r2 is needed, because a beq is executed next.
    b     .Lsrc_misaligned_tail
    #else // MEMCPY_ARMV6M_OPTIMIZE_SIZE > 0
    // No status update needed, because with size optimization > 0 it is not used.
    b     .Lsrc_misaligned_tail
    #endif
  #else // MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_2_WORDS == 0
    b     .Lsrc_misaligned_tail_add4
  #endif
    .endm

.Lmisaligned_copy_1_3:
    mis_src_copy 8
    mis_src_jump_to_adjust_size
.Lmisaligned_copy_3_1:
    mis_src_copy 24
    mis_src_jump_to_adjust_size
.Lmisaligned_copy_2_2:
    /* For 2_2 misalignment, ldr is still faster than 2 x ldrh.  */
    mis_src_copy 16

  #if MEMCPY_ARMV6M_MISALIGNED_COPY_LOOP_2_WORDS
.Lsrc_misaligned_tail_sub4:
    subs  r2, #4
  #endif
.Lsrc_misaligned_tail_add4:
    adds  r2, #4    // Calculate remaining size
.Lsrc_misaligned_tail:

// At this point, remaining bytes (in r2) must be 3 or less.
  #if !MEMCPY_ARMV6M_OPTIMIZE_SIZE
    beq   .Ldone
    subs  r1, r5    // Sub misalignment offset.

    memcpy_copy_3_2_or_1 r2
.Ldone:
    pop   {PUSHED_REGS}
    mov   r0, ip
    bx    lr
  #else // MEMCPY_ARMV6M_OPTIMIZE_SIZE > 0
    subs  r1, r5    // Sub misalignment offset.
    pop   {PUSHED_REGS}

    #if MEMCPY_ARMV6M_OPTIMIZE_SIZE > 1
    b     .Lmemcpy_short
    #else
    // Compute a jump setting PC = PC + FIXED_OFFSET - 4 * entry point
    lsls  r2, #2
    subs  r2, (.Lmemcpy_short_end - .Lmisaligned_jump_start - 4)
    negs  r2, r2
.Lmisaligned_jump_start:
    add   pc, r2 // ADD PC and MOV PC branch within Thumb state without interworking.
    #endif
  #endif
#endif

#if MEMCPY_ARMV6M_FUNCTION_END_SIGNATURE // For automated tests, this pattern is searched to get the size of this function.
.word	0xFFFFFFFF
.word	0xFFFFFFFF
.word	0x0
.word	0x0
#endif
