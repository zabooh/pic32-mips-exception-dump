/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <stdio.h>                      // printf
#include "definitions.h"                // SYS function prototypes
#include "exception_dump_mips.h"        // MIPS exception crash-dump library


// *****************************************************************************
// Section: Exception-dump output callback
// *****************************************************************************
/* Emits the stored crash dump over the same console printf() uses (stdio ->
 * xc32_monitor -> UART1). Called only on boot from normal context, never from
 * the exception handler itself. */
static void edm_console_puts ( const char *s )
{
    printf("%s", s);
}

// *****************************************************************************
// Section: Test crash trigger
// *****************************************************************************
/* TEST ONLY: set to 0 (or delete this and foo_ex) for a real build. */
#ifndef EDM_FORCE_TEST_CRASH
#define EDM_FORCE_TEST_CRASH 1
#endif

#if EDM_FORCE_TEST_CRASH
/* Deliberately provoke a MIPS CPU exception from a *named, non-inlined* function
 * that main() calls. Using a separate function makes the dump show WHERE the
 * fault happened (epc -> foo_ex) and WHO called it (ra -> main), so the analyzer
 * can resolve both the faulting function and its caller.
 *
 * The fault is a MISALIGNED 32-bit store through a near-NULL pointer. On PIC32MM
 * this near-NULL, unaligned access raises a Data bus error (cause=7). The handler
 * captures the registers into edm_dump.msg and (Mode B) resets; on reboot the
 * dump is printed over UART1. 'noinline' keeps foo_ex a real call so 'ra' points
 * back into main. */
static void __attribute__((noinline)) foo_ex ( void )
{
    volatile unsigned int *misaligned = (volatile unsigned int *)0x00000001u;
    *misaligned = 0xDEADBEEFu;      /* misaligned near-NULL store -> exception */
}
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    /* If a MIPS CPU exception was captured before the last (crash-triggered)
     * reset, print it once now that the console/UART is up, then clear it. */
    exception_dump_mips_init();
    exception_dump_mips_set_output_callback(NULL, edm_console_puts);
    exception_dump_mips_check_and_print_previous();

    printf("Hello World!\n\r");

    /* TEST ONLY: crash inside foo_ex() (called from main) so the dump shows the
     * faulting function AND its caller. Simulator flow (Mode B): fault ->
     * handler captures edm_dump.msg -> software reset -> on reboot the dump is
     * printed over UART1. To inspect without the reset, breakpoint inside
     * _general_exception_handler and watch 'edm_dump.msg'. */
#if EDM_FORCE_TEST_CRASH
    foo_ex();
#endif

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

