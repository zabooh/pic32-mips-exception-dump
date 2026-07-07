/*******************************************************************************
  exception_dump_mips_port.h

  Porting layer for the PIC32 / MIPS Exception Dump package.

  This header declares the small set of hooks that connect the portable core to
  a specific device / RTOS / console. The core never calls a UART, printf,
  SYS_CONSOLE or reset register directly; it goes through these functions or
  through the runtime callbacks registered via the public API.

  All functions here are declared __attribute__((weak)) in
  src/exception_dump_mips_port.c so you can override any of them by simply
  providing your own non-weak definition in the application. You do NOT have to
  edit the port .c file.
 *******************************************************************************/

#ifndef EXCEPTION_DUMP_MIPS_PORT_H
#define EXCEPTION_DUMP_MIPS_PORT_H

#include <stdint.h>
#include "exception_dump_mips_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Perform a device software reset. Called by the exception handler after the
 * dump has been stored (when EDM_AUTO_RESET_AFTER_CAPTURE == 1) if the
 * application has not registered a reset callback via
 * exception_dump_mips_set_reset_callback().
 *
 * The default (weak) implementation performs the standard PIC32 SYSKEY unlock +
 * RSWRST software-reset sequence. Override it if your device needs a different
 * sequence or you want to route through a framework reset service. Must not
 * return. */
void edm_port_default_reset(void);

/* OPTIONAL: return a free-running boot / crash counter or timestamp that is
 * embedded in the dump header. The default (weak) implementation returns 0.
 * Override to provide a boot counter, RTC value, or tick count. This value is
 * captured inside exception context, so it must be cheap and must not block. */
uint32_t edm_port_get_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* EXCEPTION_DUMP_MIPS_PORT_H */
