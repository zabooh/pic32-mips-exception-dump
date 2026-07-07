# Bare-metal Integration Notes

## 1. No-init / persistent linker section

The XC32 default linker scripts already contain a `.persist` section (used by
`__attribute__((persistent))`), so the simplest option is to keep the default
`EDM_PERSIST_ATTR` and change nothing.

If you instead use an explicit named section
(`EDM_PERSIST_ATTR __attribute__((section(".exception_dump_noinit")))`), add a
`NOLOAD` output section to your linker script so the runtime start-up code does
not clear it. Example snippet (place it near the other RAM sections, adjust the
region name to your script, e.g. `kseg1_data_mem` on PIC32MZ):

```ld
  /* No-init RAM retained across a warm/software reset. NOT cleared at startup. */
  .exception_dump_noinit (NOLOAD) :
  {
    . = ALIGN(4);
    _exception_dump_noinit_begin = .;
    *(.exception_dump_noinit .exception_dump_noinit.*)
    . = ALIGN(4);
    _exception_dump_noinit_end = .;
  } > kseg1_data_mem
```

> ⚠️ This snippet is a clearly-marked example. The simplest option is the stock
> XC32 `.persist` section (no linker edit). Only add this custom section if you
> deliberately choose the named-section approach, and verify the region name and
> that your crt0 does not zero it.

## 2. Reset behaviour and persistence

`edm_port_default_reset()` performs the PIC32 SYSKEY unlock + `RSWRST` software
reset. RAM (and therefore the no-init dump) is retained across this warm reset.
A power-on reset, MCLR, or brown-out may clear or corrupt RAM, so the dump is
only guaranteed after the software reset that the handler itself triggers.

## 3. Vector / startup interaction

- The `.gen_handler` section in `exception_dump_mips_vector.S` must be placed at
  (or reachable from) the CPU general-exception vector address. On XC32 the
  default startup + linker script handle this for the standard EBase/vectored
  configuration; verify your `.ld` maps `.gen_handler`.
- Only compile `exception_dump_mips_vector.S` if nothing else in your project
  defines `_general_exception_context` / `_gen_exception`.

## 4. Reducing dependencies / footprint

- Set `-DEDM_DUMP_BUFFER_SIZE=1024` to shrink the persistent buffer.
- Set `-DEDM_CAPTURE_GPR=0` if you do not control the exact vector frame layout;
  you still get Cause/EPC/Status/BadVAddr.
- The core uses `sprintf` from the standard library. If you use a size-optimised
  or no-`printf` C library, provide a minimal `sprintf`/`vsnprintf` or replace
  the formatting with hand-rolled hex conversion.

## 5. Clock-dependent UART

The optional `_simple_tlb_refill_exception_handler` in the port file uses
`U1BRG = 53` for ~115200 baud at 200 MHz. If you enable it
(`-DEDM_INSTALL_TLB_REFILL_LIVE_UART=1`), fix `U1BRG` for your PBCLK.
