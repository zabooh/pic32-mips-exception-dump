/*******************************************************************************
  exception_dump_mips_vector.S

  MIPS general-exception vector for the PIC32 / MIPS Exception Dump package.

  What it does:
    1. The CPU general-exception vector (.gen_handler) jumps here.
    2. This code pushes a fixed 140-byte frame onto the stack and saves every
       general-purpose register (plus LO/HI) at a fixed word offset.
    3. It calls the C handler _general_exception_handler (provided by
       exception_dump_mips.c), passing Cause in a0 and Status in a1.
    4. On return it restores all registers and executes 'eret'.

  IMPORTANT:
    - The 140-byte frame size and the per-register offsets MUST stay in sync
      with EDM_STACK_FRAME_BYTES and the *_IDX macros in exception_dump_mips.c.
    - Many frameworks (Harmony 3, MCC) already generate an equivalent vector
      named _general_exception_context. If yours does, do NOT also compile this
      file - you would get duplicate symbols. Use whichever one your build
      already provides; the C handler works with either as long as the frame
      layout matches.
 *******************************************************************************/

#include "xc.h"
#include <cp0defs.h>

        ##################################################################
        # General Exception Vector Handler -> jumps to _general_exception_context
        ##################################################################
        .section .gen_handler,code
        .align 2
        .set noreorder
        .ent _gen_exception

_gen_exception:
0:      addu    k1, zero, sp
        la      k0, _general_exception_context
        jr      k0
        nop

        .end _gen_exception


        .extern _general_exception_handler

        .section .text.general_exception, code
        .set noreorder
        .set noat
        .set nomips16
        .globl _general_exception_context
        .ent _general_exception_context

_general_exception_context:

        # Save off the non-callee-saved registers that may get mucked with.
        # Frame layout matches the *_IDX word offsets in exception_dump_mips.c.
        addiu sp, sp, -140
        sw $1,  4(sp)
        sw v0,  8(sp)
        sw v1, 12(sp)
        sw a0, 16(sp)
        sw a1, 20(sp)
        sw a2, 24(sp)
        sw a3, 28(sp)
        sw t0, 32(sp)
        sw t1, 36(sp)
        sw t2, 40(sp)
        sw t3, 44(sp)
        sw t4, 48(sp)
        sw t5, 52(sp)
        sw t6, 56(sp)
        sw t7, 60(sp)
        sw t8, 64(sp)
        sw t9, 68(sp)
        sw ra, 72(sp)
        mflo t0
        sw t0, 76(sp)
        mfhi t0
        sw t0, 80(sp)
        sw fp, 84(sp)
        sw gp, 88(sp)
        sw k0, 92(sp)
        sw k1, 96(sp)
        sw s0, 100(sp)
        sw s1, 104(sp)
        sw s2, 108(sp)
        sw s3, 112(sp)
        sw s4, 116(sp)
        sw s5, 120(sp)
        sw s6, 124(sp)
        sw s7, 128(sp)
        addiu k0, sp, 140       # k0 = pre-exception sp (frame base + frame size)
        sw    k0, 132(sp)       # store the true stack pointer, not the frame base

#if defined(__PIC__)
        lw      t9,%call16(_general_exception_handler)(gp)
        nop
        # Pass Cause and Status to the handler function
        mfc0 a0, _CP0_CAUSE
        mfc0 a1, _CP0_STATUS
        addu a2, $0, sp                 # a2 = base of the saved register frame (3rd arg)
        jalr    t9
        nop
#else
        la      k0, _general_exception_handler
        nop
        # Pass Cause and Status to the handler function
        mfc0 a0, _CP0_CAUSE
        mfc0 a1, _CP0_STATUS
        addu a2, $0, sp                 # a2 = base of the saved register frame (3rd arg)
        jalr    k0
        nop
#endif

        lw $1,  4(sp)
        lw v0,  8(sp)
        lw v1, 12(sp)
        lw a0, 16(sp)
        lw a1, 20(sp)
        lw a2, 24(sp)
        lw a3, 28(sp)
        # t0 restored later (used as scratch for LO/HI)
        lw t1, 36(sp)
        lw t2, 40(sp)
        lw t3, 44(sp)
        lw t4, 48(sp)
        lw t5, 52(sp)
        lw t6, 56(sp)
        lw t7, 60(sp)
        lw t8, 64(sp)
        lw t9, 68(sp)
        lw ra, 72(sp)

        lw t0, 76(sp)
        mtlo t0
        lw t0, 80(sp)
        mthi t0

        lw t0, 32(sp)

        lw fp, 84(sp)
        lw gp, 88(sp)
        lw k0, 92(sp)
        lw k1, 96(sp)
        lw s0, 100(sp)
        lw s1, 104(sp)
        lw s2, 108(sp)
        lw s3, 112(sp)
        lw s4, 116(sp)
        lw s5, 120(sp)
        lw s6, 124(sp)
        lw s7, 128(sp)
        lw sp, 132(sp)

        addiu sp, sp, 140

        ehb
        eret

        .end _general_exception_context
