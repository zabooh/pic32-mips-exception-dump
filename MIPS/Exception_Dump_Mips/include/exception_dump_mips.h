/*******************************************************************************
  exception_dump_mips.h

  Public API of the PIC32 / MIPS Exception Dump package.

  Purpose:
    Capture a MIPS CP0 / general-purpose register dump when a CPU exception
    occurs, store it in persistent (no-init) RAM, optionally reset, and print
    the stored dump over a user-provided output function on the next boot.

  Target:
    PIC32M / MIPS family only (PIC32MX / PIC32MZ / PIC32MZW1 / WFI32 / PIC32MK /
    PIC32MM), XC32 compiler. This is MIPS-specific and does NOT apply to
    Cortex-M / SAM devices.
 *******************************************************************************/

#ifndef EXCEPTION_DUMP_MIPS_H
#define EXCEPTION_DUMP_MIPS_H

#include <stdint.h>
#include <stdbool.h>
#include "exception_dump_mips_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Persistent dump structure
 *
 *     struct { char msg[EDM_DUMP_BUFFER_SIZE]; uint32_t magic; }
 * The formatted, human-readable dump text is stored in 'msg'. 'magic' is set
 * to EDM_MAGIC_CODE once a valid dump has been captured.
 * -------------------------------------------------------------------------*/
typedef struct
{
    char     msg[EDM_DUMP_BUFFER_SIZE];
    uint32_t magic;
} exception_dump_mips_t;

/* Output callback: emits one character. Provided by the application and used
 * only from normal (non-exception) context, i.e. when printing a previously
 * stored dump. Typically wraps UART TX, SYS_CONSOLE, or putchar(). */
typedef void (*edm_char_output_fn)(char c);

/* Optional output callback: emits a NUL-terminated string. If provided it is
 * preferred over the per-character callback for efficiency. */
typedef void (*edm_string_output_fn)(const char *s);

/* Optional reset callback: performs a device reset. If none is registered the
 * package falls back to edm_port_default_reset() (weak, in the port layer).
 * Must not return. */
typedef void (*edm_reset_fn)(void);

/* ---------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/

/* Initialise the module. Safe (and recommended) to call early in main() or
 * from the monitor/startup task. Does NOT clear a stored dump, so it can be
 * called after a crash-reset without losing the captured information. */
void exception_dump_mips_init(void);

/* Register the output function(s) used when printing a dump.
 * Pass NULL for a callback you do not want to use. At least one of the two
 * should be set before calling the print / check functions. */
void exception_dump_mips_set_output_callback(edm_char_output_fn char_out,
                                             edm_string_output_fn string_out);

/* Register a custom reset function used by the exception handler after a dump
 * has been captured (only relevant when EDM_AUTO_RESET_AFTER_CAPTURE == 1).
 * Pass NULL to use the built-in default software reset. */
void exception_dump_mips_set_reset_callback(edm_reset_fn reset_fn);

/* If a dump was captured before the last reset (magic == EDM_MAGIC_CODE),
 * print it via the registered output callback and clear the magic so it is
 * only shown once. Returns true if a dump was present and printed.
 * Call this once during start-up, after the console/UART is ready. */
bool exception_dump_mips_check_and_print_previous(void);

/* Print the current contents of the dump buffer immediately, regardless of the
 * magic word and without clearing it. Useful for on-demand inspection or from a
 * custom (non-resetting) handler. Returns true if something was printed. */
bool exception_dump_mips_print_current(void);

/* Clear the stored dump (magic word) so it will not be reported again. */
void exception_dump_mips_clear_saved_dump(void);

/* Return a pointer to the stored dump text if a valid dump is present
 * (magic == EDM_MAGIC_CODE), else NULL. Does NOT clear the magic. */
const char *exception_dump_mips_get_saved_text(void);

/* ---------------------------------------------------------------------------
 * Exception entry point
 *
 * When EDM_INSTALL_GENERAL_HANDLER == 1, this package provides
 * _general_exception_handler() (declared by the XC32 toolchain). The assembly
 * exception vector (src/exception_dump_mips_vector.S) calls it. You normally
 * do NOT call this yourself.
 * -------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* EXCEPTION_DUMP_MIPS_H */
