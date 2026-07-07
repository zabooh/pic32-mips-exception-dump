/*******************************************************************************
  exception_dump_mips_port.c

  Default (weak) porting layer for the PIC32 / MIPS Exception Dump package.

  Everything here is __attribute__((weak)): provide a non-weak function of the
  same name in your application to override it. You should not need to edit this
  file for a normal port.

  The reset sequence below is the standard PIC32 unlock + software-reset (RSWRST)
  sequence. It is valid for PIC32MX / PIC32MZ / PIC32MZW1 / PIC32MM / PIC32MK. If
  you prefer, override edm_port_default_reset() with a call to your framework's
  software-reset service instead.
 *******************************************************************************/

#include <xc.h>
#include <stdint.h>
#include "exception_dump_mips_port.h"

/* ---------------------------------------------------------------------------
 * Default software reset: SYSKEY unlock, then set RSWRST.SWRST and read RSWRST
 * to trigger. Loops re-issuing the sequence so it cannot fall through.
 * -------------------------------------------------------------------------*/
__attribute__((weak, noreturn)) void edm_port_default_reset(void)
{
    while (1)
    {
        /* Unlock sequence for the system-key protected registers. */
        SYSKEY = 0x00000000;
        SYSKEY = 0xAA996655;
        SYSKEY = 0x556699AA;

        RSWRSTSET = _RSWRST_SWRST_MASK;

        /* Read RSWRST to arm/execute the reset (per PIC32 family reference). */
        (void)RSWRST;

        Nop();
        Nop();
        Nop();
        Nop();
    }
}

/* ---------------------------------------------------------------------------
 * Default timestamp / boot counter: none. Override to embed a value in the
 * dump header (e.g. a boot counter stored in another persistent variable).
 * -------------------------------------------------------------------------*/
__attribute__((weak)) uint32_t edm_port_get_timestamp(void)
{
    return 0u;
}

/* ===========================================================================
 * OPTIONAL device-specific example: live TLB-refill dump over UART1.
 *
 * This writes PIC32MZ UART1 registers directly and is therefore NOT portable.
 * It is compiled only when EDM_INSTALL_TLB_REFILL_LIVE_UART == 1. It implements
 * _simple_tlb_refill_exception_handler(), which prints a short message live and
 * then resets. Kept as example / reference code.
 *
 * The register setup (U1BRG = 53) assumes a 200 MHz CPU clock for 115200 baud;
 * adjust for your clock. Do NOT enable this in a portable build.
 * ===========================================================================*/
#if (EDM_INSTALL_TLB_REFILL_LIVE_UART == 1)

#include <string.h>
#include <stdio.h>

void __attribute__((nomips16, noreturn)) _simple_tlb_refill_exception_handler(void)
{
    static char str[128];
    unsigned int badInstAddr = _CP0_GET_NESTEDEPC();
    int len;
    int ix;

    sprintf(str, "\r\n\r\nTLB Refill Exception at %08x\r\n", badInstAddr);
    len = (int)strlen(str);

    /* Minimal UART1 bring-up (PIC32MZ). Adjust U1BRG for your clock. */
    U1MODE          = 0x0;
    U1STAbits.UTXEN = 1;
    U1BRG           = 53;          /* ~115200 @ 200 MHz PBCLK2 */
    IEC1bits.U1TXIE = 0;
    U1MODESET       = _U1MODE_ON_MASK;

    for (ix = 0; ix < len; ix++)
    {
        while (U1STAbits.UTXBF == 1)
        {
            ; /* wait for TX FIFO space */
        }
        U1TXREG = str[ix];
    }

    edm_port_default_reset();
    while (1) { Nop(); }
}

#endif /* EDM_INSTALL_TLB_REFILL_LIVE_UART */

/*******************************************************************************
 End of File
 */
