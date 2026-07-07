# Example: Bare-metal PIC32 (no Harmony) with direct UART

This example shows how to use the Exception Dump package on a bare-metal XC32
project (PIC32MX / PIC32MZ) with output written straight to a UART, and with an
explicit no-init linker section.

## Files to add

- `src/exception_dump_mips.c`
- `src/exception_dump_mips_port.c`
- `src/exception_dump_mips_vector.S`  ← add this on bare metal (you provide the
  vector yourself, since there is no Harmony to generate one)

Include path: `include/`.

Make sure your MIPS exception configuration routes the general-exception vector
to `_gen_exception` / `_general_exception_context` (the `.gen_handler` section in
the `.S` file). On XC32 with the standard startup, defining
`_general_exception_handler` is enough for the default vector model.

## UART output callback (PIC32MZ UART1, polled)

```c
#include <xc.h>
#include "exception_dump_mips.h"

static void uart_putc(char c)
{
    while (U1STAbits.UTXBF)        /* wait while TX FIFO full */
        ;
    U1TXREG = (uint8_t)c;
}

void exdump_setup(void)
{
    /* ... assume UART1 already initialised for your baud/clock ... */
    exception_dump_mips_init();
    exception_dump_mips_set_output_callback(uart_putc, NULL);

    /* On boot, print any dump captured before the last reset. */
    exception_dump_mips_check_and_print_previous();
}
```

## Persistent buffer via an explicit no-init section

If you do not want to rely on the XC32 `persistent` attribute, use a named
section. Define this in your build (e.g. `-DEDM_PERSIST_ATTR=...`) or edit the
config header:

```c
#define EDM_PERSIST_ATTR __attribute__((section(".exception_dump_noinit")))
```

Then add the section to your linker script — see `integration_notes.md`.

## Triggering a test exception

```c
/* store to an illegal/misaligned address → address error exception */
volatile uint32_t *bad = (uint32_t *)0x00000001;
*bad = 0x12345678;
```

After the automatic reset, `check_and_print_previous()` prints the dump over
UART1.
