/*******************************************************************************
  exception_dump_mips.c

  Portable core of the PIC32 / MIPS Exception Dump package.

  Behaviour:
    - _general_exception_handler() reads the GPRs that the assembly exception
      vector pushed onto the stack, plus the CP0 Cause and EPC registers.
    - The full register set is formatted (sprintf) into a persistent (no-init)
      buffer that survives a soft reset.
    - A magic word marks the buffer as valid.
    - The device performs a software reset (Mode B).
    - On the next boot the application calls
      exception_dump_mips_check_and_print_previous() to print the stored dump.

  Design:
    - The handler does not touch the UART directly; all output goes through a
      user callback (see exception_dump_mips_set_output_callback).
    - The reset is done through a replaceable callback / weak port function.
    - Buffer size, section placement, stack-frame layout and the optional extra
      CP0 registers are all configurable in exception_dump_mips_config.h.
 *******************************************************************************/

#include <xc.h>                 /* SFRs + CP0 access macros (_CP0_GET_*)        */
#include <stdio.h>              /* sprintf                                      */
#include <string.h>            /* strlen                                       */
#include "exception_dump_mips.h"
#include "exception_dump_mips_port.h"

/* ===========================================================================
 * Persistent storage
 *
 * Placed in a no-init / persistent section (see EDM_PERSIST_ATTR in the config
 * header) so its contents survive the software reset that follows a crash.
 * ===========================================================================*/
exception_dump_mips_t edm_dump EDM_PERSIST_ATTR;

/* Runtime callbacks (live in normal RAM; only used from non-exception context,
 * except edm_reset_cb which is read inside the handler). */
static edm_char_output_fn   edm_char_out   = 0;
static edm_string_output_fn edm_string_out = 0;
static edm_reset_fn         edm_reset_cb    = 0;

/* ===========================================================================
 * Exception cause decode table (CP0 Cause.ExcCode)
 * Standard MIPS exception cause codes.
 * ===========================================================================*/
static const char *const edm_cause[] =
{
    "Interrupt",                 /* 0  */
    "Undefined",                 /* 1  */
    "Undefined",                 /* 2  */
    "Undefined",                 /* 3  */
    "Load/fetch address error",  /* 4  */
    "Store address error",       /* 5  */
    "Instruction bus error",     /* 6  */
    "Data bus error",            /* 7  */
    "Syscall",                   /* 8  */
    "Breakpoint",                /* 9  */
    "Reserved instruction",      /* 10 */
    "Coprocessor unusable",      /* 11 */
    "Arithmetic overflow",       /* 12 */
    "Trap",                      /* 13 */
    "Reserved",                  /* 14 */
    "Reserved",                  /* 15 */
    "Reserved",                  /* 16 */
    "Reserved",                  /* 17 */
    "Reserved"                   /* 18 */
};
#define EDM_CAUSE_COUNT  (sizeof(edm_cause) / sizeof(edm_cause[0]))

/* ===========================================================================
 * Word offsets of each saved register inside the stack frame pushed by the
 * assembly exception vector (_general_exception_context). MUST match
 * exception_dump_mips_vector.S. See EDM_STACK_FRAME_BYTES in the config header.
 * ===========================================================================*/
#define EDM_AT_IDX   1
#define EDM_V0_IDX   2
#define EDM_V1_IDX   3
#define EDM_A0_IDX   4
#define EDM_A1_IDX   5
#define EDM_A2_IDX   6
#define EDM_A3_IDX   7
#define EDM_T0_IDX   8
#define EDM_T1_IDX   9
#define EDM_T2_IDX   10
#define EDM_T3_IDX   11
#define EDM_T4_IDX   12
#define EDM_T5_IDX   13
#define EDM_T6_IDX   14
#define EDM_T7_IDX   15
#define EDM_T8_IDX   16
#define EDM_T9_IDX   17
#define EDM_RA_IDX   18
#define EDM_LO_IDX   19
#define EDM_HI_IDX   20
#define EDM_FP_IDX   21
#define EDM_GP_IDX   22
#define EDM_K0_IDX   23
#define EDM_K1_IDX   24
#define EDM_S0_IDX   25
#define EDM_S1_IDX   26
#define EDM_S2_IDX   27
#define EDM_S3_IDX   28
#define EDM_S4_IDX   29
#define EDM_S5_IDX   30
#define EDM_S6_IDX   31
#define EDM_S7_IDX   32
#define EDM_SP_IDX   33

/* ===========================================================================
 * Small internal print helper (normal context only).
 * ===========================================================================*/
static void edm_emit(const char *s)
{
    if (s == 0)
    {
        return;
    }
    if (edm_string_out != 0)
    {
        edm_string_out(s);
    }
    else if (edm_char_out != 0)
    {
        while (*s != '\0')
        {
            edm_char_out(*s++);
        }
    }
}

/* ===========================================================================
 * Public API
 * ===========================================================================*/

void exception_dump_mips_init(void)
{
    /* Nothing to initialise for the persistent buffer: it is intentionally NOT
     * cleared here so a dump captured before the reset survives. Provided as an
     * explicit hook / for symmetry and future use. */
}

void exception_dump_mips_set_output_callback(edm_char_output_fn char_out,
                                             edm_string_output_fn string_out)
{
    edm_char_out   = char_out;
    edm_string_out = string_out;
}

void exception_dump_mips_set_reset_callback(edm_reset_fn reset_fn)
{
    edm_reset_cb = reset_fn;
}

const char *exception_dump_mips_get_saved_text(void)
{
    if (edm_dump.magic == EDM_MAGIC_CODE)
    {
        return &edm_dump.msg[0];
    }
    return 0;
}

void exception_dump_mips_clear_saved_dump(void)
{
    edm_dump.magic = 0u;
}

bool exception_dump_mips_print_current(void)
{
    /* Ensure the buffer is NUL-terminated before printing. */
    edm_dump.msg[EDM_DUMP_BUFFER_SIZE - 1u] = '\0';
    if (edm_dump.msg[0] == '\0')
    {
        return false;
    }
    edm_emit(edm_dump.msg);
    edm_emit("\r\n");
    return true;
}

bool exception_dump_mips_check_and_print_previous(void)
{
    if (edm_dump.magic != EDM_MAGIC_CODE)
    {
        return false;
    }

    edm_emit("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
    edm_emit("Last runtime ended with the following exception:\r\n");
    (void)exception_dump_mips_print_current();
    edm_emit("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");

    /* Show only once. */
    edm_dump.magic = 0u;
    return true;
}

/* ===========================================================================
 * General exception handler
 *
 * Installed only when EDM_INSTALL_GENERAL_HANDLER == 1. It runs in exception
 * context: the stack may be marginal, so it uses the persistent buffer and
 * static/volatile locals rather than large stack arrays.
 * ===========================================================================*/
#if (EDM_INSTALL_GENERAL_HANDLER == 1)

/* Cause of the exception (CP0 Cause.ExcCode). Static so it does not need the
 * stack and can also be inspected from a debugger. */
static volatile unsigned int edm_excep_code;
static volatile unsigned int edm_excep_addr;

void __attribute__((nomips16)) _general_exception_handler(
        unsigned int edm_arg_cause, unsigned int edm_arg_status,
        uint32_t *edm_frame)
{
    (void)edm_arg_cause;   /* Cause/Status are re-read from CP0 below; the args */
    (void)edm_arg_status;  /* only position edm_frame as the 3rd (a2) argument. */
#if (EDM_CAPTURE_GPR == 1)
    /* The assembly vector (exception_dump_mips_vector.S) saved every GPR into a
     * fixed EDM_STACK_FRAME_BYTES frame and passes the BASE of that frame in a2
     * (this function's 3rd argument). Reading it as a function argument is
     * ABI-robust: unlike reading $sp live, it does NOT depend on the compiler-
     * generated prologue of this handler, which at -O1/-O2 adjusts $sp before
     * any C statement runs (that shift previously corrupted the whole dump). */
    const uint32_t *pul = edm_frame;
    const uint32_t s5 = (pul != 0) ? pul[EDM_S5_IDX] : 0u;
#else
    (void)edm_frame;
#endif

    /* Mask off the ExcCode field from the Cause register.
     * Refer to the MIPS Software User's Manual. */
    edm_excep_code = (_CP0_GET_CAUSE() & 0x0000007Cu) >> 2;
    edm_excep_addr = _CP0_GET_EPC();

    const char *cause_str =
        (edm_excep_code < EDM_CAUSE_COUNT) ? edm_cause[edm_excep_code]
                                           : "Unknown";

    int off = 0;
    off += sprintf(&edm_dump.msg[off],
                   "\r\n===> General Exception <===\r\n"
                   "%s (cause=%u, epc=%08x)\r\n",
                   cause_str,
                   (unsigned)edm_excep_code,
                   (unsigned)edm_excep_addr);

#if (EDM_CAPTURE_EXTRA_CP0 == 1)
    off += sprintf(&edm_dump.msg[off],
                   "status=%08x badvaddr=%08x\r\n",
                   (unsigned)_CP0_GET_STATUS(),
                   (unsigned)_CP0_GET_BADVADDR());
#endif

#if (EDM_CAPTURE_GPR == 1)
    off += sprintf(&edm_dump.msg[off],
                   "v0=%08x v1=%08x a0=%08x a1=%08x\r\n"
                   "a2=%08x a3=%08x ra=%08x s5=%08x\r\n"
                   "t0=%08x t1=%08x t2=%08x t3=%08x\r\n"
                   "t4=%08x t5=%08x t6=%08x t7=%08x\r\n"
                   "t8=%08x t9=%08x k0=%08x k1=%08x\r\n"
                   "fp=%08x gp=%08x s0=%08x s1=%08x\r\n"
                   "s2=%08x s3=%08x s4=%08x s5=%08x\r\n"
                   "s6=%08x s7=%08x sp=%08x",
                   (unsigned)pul[EDM_V0_IDX], (unsigned)pul[EDM_V1_IDX],
                   (unsigned)pul[EDM_A0_IDX], (unsigned)pul[EDM_A1_IDX],
                   (unsigned)pul[EDM_A2_IDX], (unsigned)pul[EDM_A3_IDX],
                   (unsigned)pul[EDM_RA_IDX], (unsigned)s5,
                   (unsigned)pul[EDM_T0_IDX], (unsigned)pul[EDM_T1_IDX],
                   (unsigned)pul[EDM_T2_IDX], (unsigned)pul[EDM_T3_IDX],
                   (unsigned)pul[EDM_T4_IDX], (unsigned)pul[EDM_T5_IDX],
                   (unsigned)pul[EDM_T6_IDX], (unsigned)pul[EDM_T7_IDX],
                   (unsigned)pul[EDM_T8_IDX], (unsigned)pul[EDM_T9_IDX],
                   (unsigned)pul[EDM_K0_IDX], (unsigned)pul[EDM_K1_IDX],
                   (unsigned)pul[EDM_FP_IDX], (unsigned)pul[EDM_GP_IDX],
                   (unsigned)pul[EDM_S0_IDX], (unsigned)pul[EDM_S1_IDX],
                   (unsigned)pul[EDM_S2_IDX], (unsigned)pul[EDM_S3_IDX],
                   (unsigned)pul[EDM_S4_IDX], (unsigned)pul[EDM_S5_IDX],
                   (unsigned)pul[EDM_S6_IDX], (unsigned)pul[EDM_S7_IDX],
                   (unsigned)pul[EDM_SP_IDX]);
#endif
    (void)off;

    edm_dump.msg[EDM_DUMP_BUFFER_SIZE - 1u] = '\0';
    edm_dump.magic = EDM_MAGIC_CODE;

#if (EDM_AUTO_RESET_AFTER_CAPTURE == 1)
    if (edm_reset_cb != 0)
    {
        edm_reset_cb();
    }
    else
    {
        edm_port_default_reset();
    }
#endif

    /* Fallback: never return from the exception. */
    while (1)
    {
        Nop();
    }
}

#endif /* EDM_INSTALL_GENERAL_HANDLER */

/*******************************************************************************
 End of File
 */
