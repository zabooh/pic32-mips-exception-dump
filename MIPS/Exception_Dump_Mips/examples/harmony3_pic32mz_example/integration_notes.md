# Harmony 3 Integration Notes

## 1. Files to add / exclude in MPLAB X

Add to the project (Projects → Source Files):
- `src/exception_dump_mips.c`
- `src/exception_dump_mips_port.c`

Do **not** add:
- `src/exception_dump_mips_vector.S` — Harmony already generates the vector.

Exclude / remove (to avoid a duplicate `_general_exception_handler`):
- `firmware/src/config/<config>/exceptions.c` — the exception file your MHC
  configuration generates (the exact name may vary by configuration).

Alternative without touching generated files: set
`-DEDM_INSTALL_GENERAL_HANDLER=0` and keep Harmony's handler; you then only get
the print/persistent helpers, not the register dump (unless you copy the capture
call into the generated handler).

Add the include path: `include/` (Project Properties → xc32-gcc → Preprocessing
and messages → Include directories).

## 2. Regeneration warning

If you re-run MHC code generation, Harmony may recreate `exceptions.c`.
Re-apply the exclusion, or keep the package's handler in a
separate non-generated folder and keep MHC's handler disabled. Consider adding a
short note in your project's `README` so the exclusion is not lost on the next
regen.

## 3. Frame-layout check

This package assumes the Harmony `_general_exception_context` frame is 140 bytes
with the classic register offsets (`v0` at 8(sp), `a0` at 16(sp), … `sp` at
132(sp)). This has been stable across Harmony 3 PIC32MZ releases, but if a future
generated vector differs, either update `EDM_STACK_FRAME_BYTES` + the `*_IDX`
macros in `exception_dump_mips.c`, or set `-DEDM_CAPTURE_GPR=0` to dump only the
CP0 registers (Cause/EPC/Status/BadVAddr), which never depends on the frame.

## 4. Output timing

`SYS_CONSOLE_PRINT` is buffered/async in Harmony. `check_and_print_previous()`
enqueues the text; make sure the console task keeps running long enough after
the call for the FIFO to drain, or use a blocking console write in your callback
if you print very early in boot.

## 5. Reset service

The built-in `edm_port_default_reset()` performs the SYSKEY/RSWRST sequence
directly. If you prefer Harmony's service, register it:
```c
exception_dump_mips_set_reset_callback(SYS_RESET_SoftwareReset);
```
Both end in an RSWRST soft reset, which preserves `.persist`.
