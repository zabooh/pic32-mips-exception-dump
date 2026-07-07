# Reading the Exception Dump & Locating It in the Listing

This guide explains how to turn a captured exception dump (from Mode A watch
window or Mode B UART output) into an exact **source line + instruction** using
the disassembly listing produced by [`create_listing.bat`](create_listing.bat).

---

## 1. Anatomy of the dump

A captured dump looks like this (fields depend on your config):

```
===> General Exception <===
Store address error (cause=5, epc=9d0011a4)
status=00100003 badvaddr=00000000
v0=00000000 v1=a0000000 a0=00000000 a1=deadbeef
a2=... a3=... ra=9d000f30 s5=...
t0=... t1=... ... sp=a0007e80
...
```

| Field | Meaning | Use it to... |
|-------|---------|--------------|
| `cause` | CP0 Cause.ExcCode + decoded text | understand **what** went wrong |
| **`epc`** | Exception Program Counter | find the **faulting instruction** |
| `status` | CP0 Status | check mode/interrupt state (optional) |
| `badvaddr` | CP0 BadVAddr | the **bad address** for load/store address errors |
| `ra` | return address ($31) | find the **caller** (one level up the call stack) |
| `sp`, `s0..s7`, `gp` | saved registers | deeper stack analysis / variable values |

The most important value is **`epc`** — it is (almost always) the address of the
instruction that caused the exception.

> **Branch delay slot:** if the faulting instruction was in a branch delay slot,
> `epc` points at the *branch* and the CPU sets `Cause.BD`. If `epc` looks like a
> branch/jump, the real fault is the following instruction.

---

## 2. KSEG address note (important)

MIPS/PIC32 virtual addresses carry a region in the top nibble:

- `0x9Dxxxxxx` = KSEG0 **cached** program flash
- `0x8Dxxxxxx` = KSEG0 flash (other mapping) / `0xBDxxxxxx` = KSEG1 **uncached**
- `0x9Fxxxxxx` / `0xBFxxxxxx` = boot flash, `0x00/0x80/0xA0...` = RAM

`xc32-objdump` prints addresses in the same virtual form used at run time, so an
`epc` of `9d0011a4` usually appears **verbatim** in the listing. If the top
nibble differs (cached vs uncached alias of the same physical memory), **match
on the lower ~6-7 hex digits** (`0011a4`).

---

## 3. Generate the listing

Run the batch (adapt the paths first, or let Claude Code adapt it):

```
create_listing.bat  path\to\MyProject.X.production.elf
```

This produces next to the ELF:
- `*.disassembly.txt` — source-interleaved disassembly (`xc32-objdump -d -S -l`)
- `*.symbols.txt` — symbol table (`xc32-readelf -s`)

> **Tip:** build/keep a **debug** ELF for the best C-source interleaving. In an
> optimized production build the source lines are sparser but the instruction
> addresses are still exact.

---

## 4. Find the faulting instruction (`epc`)

Open `*.disassembly.txt` and **search for the `epc` address** (e.g. `11a4:` or
`9d0011a4`). You will land on something like:

```
9d0011a0 <hal_write_reg>:
   ...
   9d0011a0:  8fc40010    lw   a0,16(s8)
   9d0011a4:  ac820000    sw   v0,0(a0)      ; <-- epc: store to address in a0
   9d0011a8:  03e00008    jr   ra
```

Because the listing was built with `-S`, the corresponding **C source line** is
shown just above the machine instructions, e.g.:

```
void hal_write_reg(reg_t *r, uint32_t v) {
    *r = v;            // <-- this line faulted
}
```

So: the fault is a `sw` (store word) to the address in `a0`, and from the dump
`a0 = 00000000` and `badvaddr = 00000000` → a **NULL pointer store**.

---

## 5. Find the enclosing function and the caller (`ra`)

- **Which function?** Scroll up from the `epc` line to the nearest `<name>:`
  label (here `hal_write_reg`), or look the address up in `*.symbols.txt`.
- **Who called it?** Search the listing for the **`ra`** address the same way.
  `ra = 9d000f30` might land inside `app_task`, right after a
  `jal hal_write_reg` — that is the call site. Repeat with saved `s`-registers /
  values on the stack to walk further up if needed.

This gives you a practical call chain: `app_task → hal_write_reg → (fault)`.

---

## 6. Cause quick reference (CP0 Cause.ExcCode)

| code | meaning | typical root cause |
|-----:|---------|--------------------|
| 4 | Load/fetch address error (AdEL) | bad/misaligned read pointer, bad PC |
| 5 | Store address error (AdES) | bad/misaligned write pointer (often NULL) |
| 6 | Instruction bus error (IBE) | jump through a bad function pointer |
| 7 | Data bus error (DBE) | access to non-existent peripheral/RAM |
| 8 | Syscall | `syscall` executed |
| 9 | Breakpoint | `break` / debugger |
| 10 | Reserved instruction (RI) | corrupted code / bad function pointer |
| 11 | Coprocessor unusable (CpU) | FPU used without enable, etc. |
| 12 | Arithmetic overflow | trapping arithmetic |
| 13 | Trap | e.g. divide checks |

For address errors (4/5) always cross-check **`badvaddr`** against the pointer
register used by the faulting instruction.

---

## 7. Checklist

1. Read `cause` → what kind of fault.
2. Take `epc` → search `*.disassembly.txt` → faulting instruction + C line.
3. For address errors, compare `badvaddr` with the instruction's pointer reg.
4. Take `ra` → search listing → the caller / call site.
5. Use `*.symbols.txt` to name any raw address.
6. Mind the branch-delay-slot and KSEG-nibble notes above.
