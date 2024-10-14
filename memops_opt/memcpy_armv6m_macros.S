/*
 * Copyright (c) 2024 Visenri.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Helper macros for optimized memcpy for ARM Cortex-M0/M0+ (little endian).
 */

/* ---------------------------------------------------------------
  Macro to copy 3, 2, or 1 byte(s):
    count_reg: register with the size to copy.
    r0: destination pointer.
    r1: source pointer.
    r3: scratch register used for the copy.
  ---------------------------------------------------------------*/
.macro memcpy_copy_3_2_or_1 count_reg
LOCAL copy_2, copy_1

    cmp   \count_reg, #2
    blo   copy_1  // < 2, must be 1
    beq   copy_2  // == 2

// Default, > 2, must be 3
    ldrb  r3, [r1, #2]
    strb  r3, [r0, #2]
copy_2:
    ldrb  r3, [r1, #1]
    strb  r3, [r0, #1]
copy_1:
    ldrb  r3, [r1, #0]
    strb  r3, [r0, #0]
.endm

/* ---------------------------------------------------------------
  Macro to copy aligned or misaligned data, aligning at source:
    Medium speed, upwards copy.
    Minimum copy size: 7.
    Focus on using ldr/ldmia/ldrh at source.
    This results in a good all-round speed/size tradeof.
    It works as fast as possible with XIP memory because of the source alignment.

    copy_tail_and_return: 
        1: All bytes copied, including unaligned tail, return when done.
        0: Unaligned tail not copied, no return.
    optimize_size:
        Select size optimization level, trading size for speed.
        0 and 1 share the same copy loop, the difference is in the pre-alignment instructions
            0: LONGEST - FASTEST VERSION
            1: MEDIUM SIZED - QUITE FAST VERSION
            2: SMALLEST - SLOWEST VERSION
    r0: destination pointer (in-out).
    r1: source pointer (in-out).
    r2: copy size  (in-out).
    r3: scratch register used for the copy.
    ip: value to return in r0.
  ---------------------------------------------------------------*/
.macro memcpy_copy_src_aligned copy_tail_and_return, optimize_size

.if optimize_size < 1
// LONGEST - FASTEST VERSION
LOCAL src_align1, src_align_test_1_or_2, src_align_done, copy_word_loop, done
    lsls  r3, r1, #31
    bcs   src_align_test_1_or_2
    // 0 or 3
    beq   src_align_done    // 0

// src_align3
    subs  r1, #1
    ldmia r1!, {r3}
    .irp offset,0,1,2
    lsrs  r3, #8
    strb  r3, [r0, #\offset]
    .endr
    adds  r0, #3
    subs  r2, #3 + 4
    b     copy_word_loop

src_align_test_1_or_2:
    bne   src_align1  // == 1
    
// src_align2
    ldrh  r3, [r1]
    strb  r3, [r0, #0]
    lsrs  r3, #8
    strb  r3, [r0, #1]
    adds  r1, #2
    adds  r0, #2
    subs  r2, #2 + 4
    b     copy_word_loop
src_align1:
    ldrb  r3, [r1]
    strb  r3, [r0]
    adds  r1, #1
    adds  r0, #1
    subs  r2, #1

    // Source is aligned, read using ldmia, store with strb.
src_align_done:
    subs  r2, #4
copy_word_loop:
    ldmia r1!, {r3}
    strb  r3, [r0]
    .irp offset,1,2,3
    lsrs  r3, #8
    strb  r3, [r0, #\offset]
    .endr
    adds  r0, #4    // Next word address, r1 has already been incremented by ldmia.
    subs  r2, #4    // Sub size.
    bhs   copy_word_loop

.elseif optimize_size < 2
// MEDIUM SIZED - QUITE FAST VERSION
LOCAL copy_loop
    push  {r4}

    subs  r2, #4    // Decrease size, assume we will read 4 bytes, corrected later if needed.

    movs  r4, #3    // Calculate misalignment -> unneeded bytes.
    ands  r4, r1
    beq   copy_loop // Aligned.

    subs  r1, r4    // Remove misalignment
    subs  r0, r4    // Move r0 by the same amount to compensate the fixed strb offset.
    adds  r2, r4    // Add unused bytes to r2, to fix the initial sub of 4.
                    // The resulting equation is: r2 - 4 + unused = r2 - (4 - unused) = r2 - used
    
    ldmia r1!, {r3} // Load unaligned data and increment r1.
    lsls  r4, #3    // Calculate shift to remove unneeded bytes.
    lsrs  r3, r4    // Right align data, shift out unneeded bytes.

    // Compute a jump setting PC = PC + 4 * entry point
    lsrs  r4, #1    // Unneeded bytes * 4 => [1, 2, 3] * 4
    add   pc, r4    // ADD PC and MOV PC branch within Thumb state without interworking.
    // VERY IMPORTANT!!! For the computed jump to work as intended:
    // There must be exactly 3 instructions between the "add pc" and the "Entry point for misalignment 1".
copy_loop:
    ldmia r1!, {r3}
    strb  r3, [r0, #0]  // This would be the entry point for pc + 0.
    .irp offset,1,2,3
    lsrs  r3, #8
    strb  r3, [r0, #\offset]  // Entry point for misalignment 1 (offset 1).
    .endr
    adds  r0, #4    // Next word address, r1 has already been incremented by ldmia.
    subs  r2, #4    // Sub size.
    bhs   copy_loop

    pop   {r4}
.else //----------------------------------------------------------------------------------------
// SMALLEST - SLOWEST VERSION
    push  {r4}
// ---------------------------------------------------------------
// Check source alignment, load required data from memory (to r3),
// initialize r4 with the bytes read, and update size (r2).
    movs  r4, #3    // Calculate misalignment -> unneeded bytes.
    ands  r4, r1
    subs  r1, r4    // Remove misalignment

    ldmia r1!, {r3}

    lsls  r4, #3    // Calculate shift to remove unneeded bytes.
    lsrs  r3, r4    // Shift out unneeded bytes.

    lsrs  r4, #3    // Get back the unneeded bytes.
    subs  r4, #4    // r4 = unneeded bytes - 4 = -used bytes.

    adds  r2, r4    // sub used bytes from size.

// ---------------------------------------------------------------
// Unaligned source data has already been read (count in r4).
// Data (r3) is right aligned, store it using strb.
// Then read data with ldmia and store it with strb.
LOCAL long_copy_loop, store_loop_init, store_loop    
    b     store_loop_init
    // Source is aligned, read using ldmia, store with strb.
long_copy_loop:
    ldmia r1!, {r3}

    movs  r4, #4    // 4 bytes to copy.
    negs  r4, r4
store_loop_init:
    subs  r0, r4    // Add used bytes
store_loop:
    strb  r3, [r0, r4]
    lsrs  r3, #8
    adds  r4, #1
    bne   store_loop
    
    subs  r2, #4
    bhs   long_copy_loop

    pop   {r4}
.endif

    adds  r2, #4    // Calculate remaining size.

.if copy_tail_and_return
    beq   done
1:
    ldr   r3, [r1]
    strb  r3, [r0, #0]
    subs  r2, #1
    beq   done
    lsrs  r3, #8
    strb  r3, [r0, #1]
    subs  r2, #1
    beq   done
    lsrs  r3, #8
    strb  r3, [r0, #2] 
done:
    mov   r0, ip
    bx    lr
.endif
.endm
