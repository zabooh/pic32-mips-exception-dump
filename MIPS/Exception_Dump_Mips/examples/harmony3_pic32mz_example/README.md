# Example: Harmony 3 (PIC32MZ / PIC32MZW1 / WFI32) with SYS_CONSOLE

This example shows how to wire the Exception Dump package into a Harmony 3
project that uses `SYS_CONSOLE` for output (Mode B).

## Key point about the exception vector

Harmony 3 already generates the general-exception vector
(`_general_exception_context`). Therefore:

- **Do NOT** add `src/exception_dump_mips_vector.S` to the project.
- **DO** remove/exclude the generated exception file (typically `exceptions.c`)
  so that this package's `_general_exception_handler` is the only one, OR set
  `EDM_INSTALL_GENERAL_HANDLER 0` and keep the generated handler while using only
  the persistent-storage + print helpers.

The register stack-frame offsets in this package match the standard Harmony
`_general_exception_context` 140-byte frame.

## Output wiring (SYS_CONSOLE)

```c
#include "definitions.h"                 /* Harmony: SYS_CONSOLE_PRINT      */
#include "exception_dump_mips.h"

static void edm_puts(const char *s)
{
    SYS_CONSOLE_PRINT("%s", s);
}

void app_exception_dump_setup(void)
{
    exception_dump_mips_init();
    exception_dump_mips_set_output_callback(NULL, edm_puts);
    /* Reset: keep the built-in SYSKEY/RSWRST default, or route to Harmony:   */
    /* exception_dump_mips_set_reset_callback(SYS_RESET_SoftwareReset);       */
}
```

## Print the previous dump on boot

Call this from your monitor/console task once the console is up:

```c
/* after SYS_CONSOLE is ready */
exception_dump_mips_check_and_print_previous();
```

## Persistent section

No linker changes needed: the stock XC32 PIC32MZ linker script already provides
`.persist`, and `EDM_PERSIST_ATTR` defaults to `__attribute__((persistent))`.

See `integration_notes.md` for MHC/MPLAB X specifics and conflict resolution.
