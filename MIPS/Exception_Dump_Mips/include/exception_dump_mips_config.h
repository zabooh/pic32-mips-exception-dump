/*******************************************************************************
  exception_dump_mips_config.h

  Compile-time configuration for the PIC32 MIPS Exception Dump package.

  Every option below has a safe default. Override any value by defining it in
  your own build (compiler -D flag or a project config header) BEFORE this file
  is included, or by editing the defaults here.

  Search this file for "ADAPT" to find the settings you most likely need to
  touch for a new target.
 *******************************************************************************/

#ifndef EXCEPTION_DUMP_MIPS_CONFIG_H
#define EXCEPTION_DUMP_MIPS_CONFIG_H

/* ---------------------------------------------------------------------------
 * 0. Target architecture
 *
 * Select / auto-detect the PIC32 family and apply its architecture-specific
 * defaults FIRST. Selecting PIC32MM here (or via -DEDM_TARGET_PIC32MM) switches
 * on the microMIPS / small-RAM / no-TLB / CP0-only adaptations. Because that
 * header is included before the defaults below, and every option uses an
 * #ifndef guard, the target-specific values take precedence over the generic
 * fall-backs in this file, while your own -D overrides still win over both.
 * See exception_dump_mips_target.h.
 * -------------------------------------------------------------------------*/
#include "exception_dump_mips_target.h"

/* ---------------------------------------------------------------------------
 * 1. Persistent dump buffer
 * -------------------------------------------------------------------------*/

/* Size of the text buffer that holds the formatted register dump.
 * NOTE: the effective default is normally set per target in
 * exception_dump_mips_target.h (e.g. 4096 on PIC32MZ, 512 on PIC32MM). The
 * value below is only a last-resort fall-back. The full GPR dump needs ~700
 * bytes; a CP0-only dump fits in ~256. */
#ifndef EDM_DUMP_BUFFER_SIZE
#define EDM_DUMP_BUFFER_SIZE            4096u
#endif

/* Magic word written into the persistent structure once a dump has been
 * captured. On the next boot the package looks for this value to decide
 * whether a previous dump is present. Any non-trivial constant works. */
#ifndef EDM_MAGIC_CODE
#define EDM_MAGIC_CODE                 0x47114711u
#endif

/* ADAPT: section placement of the persistent structure.
 *
 * The persistent structure MUST live in memory that the C start-up code does
 * NOT clear, and that survives a soft reset (RSWRST). Pick ONE of the two:
 *
 *   a) XC32 built-in "persistent" attribute (RECOMMENDED for XC32).
 *      The default XC32 linker scripts already provide the ".persist"
 *      section and the start-up code preserves it.
 *
 *   b) A named no-init section that you add to your own linker script, e.g.
 *      .exception_dump_noinit (NOLOAD). Use this when you are not on XC32, or
 *      when you want an explicit, self-documenting section. See
 *      examples/baremetal_pic32_example/integration_notes.md for a linker
 *      snippet.
 */
#ifndef EDM_PERSIST_ATTR
  #define EDM_PERSIST_ATTR             __attribute__((persistent))          /* (a) XC32 default */
  /* #define EDM_PERSIST_ATTR __attribute__((section(".exception_dump_noinit"))) */ /* (b) custom */
#endif

/* ---------------------------------------------------------------------------
 * 2. Exception handler installation
 * -------------------------------------------------------------------------*/

/* If 1, this package DEFINES _general_exception_handler() and captures a full
 * dump. Set to 0 if your project already defines _general_exception_handler
 * (for example Harmony's generated exceptions.c) and you only want to use the
 * post-reboot print / persistent-storage helpers. If left at 1, you MUST make
 * sure no other translation unit defines _general_exception_handler, otherwise
 * the linker reports a duplicate symbol. See README "Integration Steps".      */
#ifndef EDM_INSTALL_GENERAL_HANDLER
#define EDM_INSTALL_GENERAL_HANDLER    1
#endif

/* If 1, the general exception handler triggers a software reset after storing
 * the dump (Mode B: store-then-reset, print on next boot).
 * If 0, the handler stays in an endless loop after capturing so a debugger can
 * be attached (Mode A: read edm_dump.msg in a Watch window, or print live via
 * exception_dump_mips_print_current from a custom handler). */
#ifndef EDM_AUTO_RESET_AFTER_CAPTURE
#define EDM_AUTO_RESET_AFTER_CAPTURE   1
#endif

/* ---------------------------------------------------------------------------
 * 3. Stack-frame layout produced by the assembly exception vector
 * -------------------------------------------------------------------------*/

/* ADAPT (only if you change the assembly vector):
 *
 * The general-purpose registers are recovered by reading the stack frame that
 * the assembly exception vector (_general_exception_context) pushed before
 * calling _general_exception_handler. The shipped vector reserves 140 bytes and
 * stores each register at a fixed word offset.
 *
 * EDM_STACK_FRAME_BYTES  : total bytes the vector subtracted from sp
 * *_REG_IDX              : word index of each register inside that frame
 *
 * These values MUST match the assembly vector byte-for-byte. The defaults below
 * correspond 1:1 to the shipped exception_dump_mips_vector.S. If you use a
 * different exception vector, update BOTH files together, or disable GPR capture
 * with EDM_CAPTURE_GPR 0.                                                       */
#ifndef EDM_STACK_FRAME_BYTES
#define EDM_STACK_FRAME_BYTES          140
#endif

/* If 0, only the CP0 registers (Cause / EPC / Status / BadVAddr) are dumped and
 * the GPRs are skipped. Use this when you cannot guarantee the stack-frame
 * layout above (safer / more portable, less information).
 * NOTE: the effective default is normally set per target in
 * exception_dump_mips_target.h (1 on PIC32MZ/MX, 0 on PIC32MM/GENERIC). The
 * value below is only a last-resort fall-back. */
#ifndef EDM_CAPTURE_GPR
#define EDM_CAPTURE_GPR                1
#endif

/* ---------------------------------------------------------------------------
 * 4. Optional extras
 * -------------------------------------------------------------------------*/

/* If 1, the dump also prints the CP0 Status and BadVAddr registers in addition
 * to Cause and EPC. They are almost always useful and cost nothing extra. Set
 * to 0 for a minimal Cause/EPC-only dump. */
#ifndef EDM_CAPTURE_EXTRA_CP0
#define EDM_CAPTURE_EXTRA_CP0          1
#endif

/* If 1, a simple TLB-refill handler that prints live over UART1 registers is
 * compiled. This is device-specific (PIC32MZ UART1) and provided as an example
 * only; keep it OFF in portable builds. See src/exception_dump_mips_port.c. */
#ifndef EDM_INSTALL_TLB_REFILL_LIVE_UART
#define EDM_INSTALL_TLB_REFILL_LIVE_UART 0
#endif

#endif /* EXCEPTION_DUMP_MIPS_CONFIG_H */
