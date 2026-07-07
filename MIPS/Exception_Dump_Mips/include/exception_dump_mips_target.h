/*******************************************************************************
  exception_dump_mips_target.h

  Target-architecture configuration for the PIC32 / MIPS Exception Dump package.

  This header selects the target PIC32 family and, based on that selection,
  pre-sets the architecture-dependent defaults (GPR capture, dump-buffer size,
  TLB handler, microMIPS flag, ...). It is included FIRST by
  exception_dump_mips_config.h, so anything set here wins over the generic
  fall-back defaults in that file, while an explicit value you define in your
  own build still overrides everything (all defaults use #ifndef guards).

  HOW TO USE
  ----------
  1. Do nothing  -> the target is auto-detected from the XC32 device macros
                    (__PIC32MM__ / __PIC32MZ__ / __PIC32MX__ / __PIC32MK__).
  2. Or force it -> define exactly ONE of the EDM_TARGET_* macros in your build
                    (e.g. -DEDM_TARGET_PIC32MM) or by editing the "manual
                    override" line below.

  Selecting PIC32MM automatically applies the microMIPS / small-RAM / no-TLB
  adaptations (see the PIC32MM block).
 *******************************************************************************/

#ifndef EXCEPTION_DUMP_MIPS_TARGET_H
#define EXCEPTION_DUMP_MIPS_TARGET_H

/* Pull in the XC32 device macros (__PIC32MM__ etc.) for auto-detection. */
#if defined(__XC32) || defined(__XC32__)
#include <xc.h>
#endif

/* ===========================================================================
 * 1. Target selection
 *
 * ADAPT: to force a target, uncomment ONE of the following (or pass it as a
 * compiler -D flag). Leave all commented to auto-detect.
 * ===========================================================================*/
/* #define EDM_TARGET_PIC32MM   1 */
/* #define EDM_TARGET_PIC32MX   1 */
/* #define EDM_TARGET_PIC32MZ   1 */
/* #define EDM_TARGET_PIC32MZW1 1 */   /* WFI32 / PIC32MZ W1 - same core as MZ */
/* #define EDM_TARGET_GENERIC   1 */   /* unknown MIPS core - safest defaults  */

/* ---------------------------------------------------------------------------
 * Auto-detect from XC32 predefined device macros if nothing was selected.
 * -------------------------------------------------------------------------*/
#if !defined(EDM_TARGET_PIC32MM)   && !defined(EDM_TARGET_PIC32MX) && \
    !defined(EDM_TARGET_PIC32MZ)   && !defined(EDM_TARGET_PIC32MZW1) && \
    !defined(EDM_TARGET_PIC32MK)   && !defined(EDM_TARGET_GENERIC)

  #if   defined(__PIC32MM__) || defined(__PIC32MM)
    #define EDM_TARGET_PIC32MM   1
  #elif defined(__PIC32MZ__) || defined(__PIC32MZ)
    #define EDM_TARGET_PIC32MZ   1        /* covers PIC32MZ and PIC32MZ W1/WFI32 */
  #elif defined(__PIC32MX__) || defined(__PIC32MX)
    #define EDM_TARGET_PIC32MX   1
  #elif defined(__PIC32MK__) || defined(__PIC32MK)
    #define EDM_TARGET_PIC32MK   1
  #else
    #define EDM_TARGET_GENERIC   1        /* unknown - fall back to safe defaults */
  #endif

#endif

/* ===========================================================================
 * 2. Per-target adaptations
 *
 * Each block sets ONLY architecture-dependent defaults, all with #ifndef so a
 * value you defined yourself is never overwritten. Generic (device-independent)
 * options stay in exception_dump_mips_config.h.
 * ===========================================================================*/

/* -------- PIC32MM : MIPS32 microAptiv UC, microMIPS-only, no TLB, small RAM --*/
#if defined(EDM_TARGET_PIC32MM)

  #define EDM_TARGET_NAME "PIC32MM"

  /* PIC32MM executes microMIPS only. Informational flag for the port/vector. */
  #ifndef EDM_ARCH_MICROMIPS
  #define EDM_ARCH_MICROMIPS 1
  #endif

  /* The 140-byte GPR stack frame is defined by the PIC32MZ-style assembly
   * vector. On PIC32MM the toolchain-provided exception context may use a
   * different layout, so GPR capture is DISABLED by default. You still get the
   * CP0 registers (Cause/EPC/Status/BadVAddr), which never depend on the frame.
   * Re-enable with -DEDM_CAPTURE_GPR=1 ONLY after verifying the frame layout on
   * the actual device (and supplying a microMIPS-assembled matching vector). */
  #ifndef EDM_CAPTURE_GPR
  #define EDM_CAPTURE_GPR 1
  #endif

  /* PIC32MM parts have little RAM (often 8-32 KB): keep the persistent buffer
   * small. CP0-only output fits comfortably in 512 bytes. */
  #ifndef EDM_DUMP_BUFFER_SIZE
  #define EDM_DUMP_BUFFER_SIZE 512u
  #endif

  /* microAptiv UC has no TLB (fixed memory map) -> no TLB-refill exception. */
  #ifndef EDM_INSTALL_TLB_REFILL_LIVE_UART
  #define EDM_INSTALL_TLB_REFILL_LIVE_UART 0
  #endif

/* -------- PIC32MX : MIPS32 M4K, classic MIPS32, moderate RAM ---------------*/
#elif defined(EDM_TARGET_PIC32MX)

  #define EDM_TARGET_NAME "PIC32MX"
  #ifndef EDM_ARCH_MICROMIPS
  #define EDM_ARCH_MICROMIPS 0
  #endif
  #ifndef EDM_CAPTURE_GPR
  #define EDM_CAPTURE_GPR 1
  #endif
  #ifndef EDM_DUMP_BUFFER_SIZE
  #define EDM_DUMP_BUFFER_SIZE 1024u
  #endif

/* -------- PIC32MZ / PIC32MZ W1 (WFI32) : MIPS32 microAptiv/M-class, big RAM -*/
#elif defined(EDM_TARGET_PIC32MZ) || defined(EDM_TARGET_PIC32MZW1)

  #if defined(EDM_TARGET_PIC32MZW1)
    #define EDM_TARGET_NAME "PIC32MZW1"
  #else
    #define EDM_TARGET_NAME "PIC32MZ"
  #endif
  #ifndef EDM_ARCH_MICROMIPS
  #define EDM_ARCH_MICROMIPS 0
  #endif
  #ifndef EDM_CAPTURE_GPR
  #define EDM_CAPTURE_GPR 1            /* matches the shipped assembly vector */
  #endif
  #ifndef EDM_DUMP_BUFFER_SIZE
  #define EDM_DUMP_BUFFER_SIZE 4096u   /* large RAM available */
  #endif

/* -------- PIC32MK : MIPS32 microAptiv, classic MIPS32 ----------------------*/
#elif defined(EDM_TARGET_PIC32MK)

  #define EDM_TARGET_NAME "PIC32MK"
  #ifndef EDM_ARCH_MICROMIPS
  #define EDM_ARCH_MICROMIPS 0
  #endif
  #ifndef EDM_CAPTURE_GPR
  #define EDM_CAPTURE_GPR 1
  #endif
  #ifndef EDM_DUMP_BUFFER_SIZE
  #define EDM_DUMP_BUFFER_SIZE 2048u
  #endif

/* -------- GENERIC / unknown MIPS : safest, most portable defaults ----------*/
#else /* EDM_TARGET_GENERIC */

  #ifndef EDM_TARGET_NAME
  #define EDM_TARGET_NAME "GENERIC-MIPS"
  #endif
  #ifndef EDM_ARCH_MICROMIPS
  #define EDM_ARCH_MICROMIPS 0
  #endif
  #ifndef EDM_CAPTURE_GPR
  #define EDM_CAPTURE_GPR 0            /* no assumption about the stack frame */
  #endif
  #ifndef EDM_DUMP_BUFFER_SIZE
  #define EDM_DUMP_BUFFER_SIZE 512u
  #endif

#endif

/* Ensure the microMIPS flag is always defined (0 if not set above). */
#ifndef EDM_ARCH_MICROMIPS
#define EDM_ARCH_MICROMIPS 0
#endif

#endif /* EXCEPTION_DUMP_MIPS_TARGET_H */
