# CLAUDE.md — PIC32 / MIPS Exception Dump

> This file is auto-loaded when a user opens Claude Code in the **root of this
> repo**. It tells you (Claude) what this repo is and how to help the user add
> the exception-dump library to their own PIC32/MIPS firmware project.

---

## 0. Repository layout (read this first)

This repo bundles two things:

- **The library** — the reusable exception-dump code — in
  **`MIPS/Exception_Dump_Mips/`** (`include/`, `src/`, `tools/`, `examples/`,
  `docs/`).
- **A runnable PIC32MM example** — a Harmony 3 project (`pic32mm_app.X/` + `src/`)
  that integrates the library and deliberately crashes in `foo_ex()` so the whole
  capture → reset → print → analyze flow can be played through in the MPLAB X
  Simulator. The full documentation is the repo-root [`README.md`](README.md).

**Path convention in this file:** references like `include/…`,
`src/exception_dump_mips*`, `tools/…`, `examples/…` are shown with the
**`MIPS/Exception_Dump_Mips/`** prefix — they mean the library copy. The
**repo-root `src/` and `pic32mm_app.X/` are the example app, not the library.**

---

## 1. What the library is (say this to the user up front)

`MIPS/Exception_Dump_Mips/` is a **reusable, portable C library that captures a
MIPS/PIC32 CPU exception (crash) dump** — the CP0 registers (`Cause`, `EPC`,
`Status`, `BadVAddr`) and, where possible, the general-purpose registers — and
makes them available for post-mortem debugging.

It targets the **PIC32M / MIPS family only** (PIC32MX / PIC32MZ / PIC32MZW1 /
WFI32 / PIC32MK / PIC32MM), XC32 compiler. It is **not** for Cortex-M / SAM
parts.

It is framework-independent: the core depends only on XC32 + the device header
(`<xc.h>`), not on Harmony, MCC or Melody. Output and reset are provided by the
application through callbacks / weak port functions.

---

## 2. How to behave when a user opens this repo

On your **first response** in a fresh session here, proactively:

1. In 2–3 sentences, tell the user this repo contains the PIC32/MIPS Exception
   Dump library (in `MIPS/Exception_Dump_Mips/`) plus a runnable PIC32MM example
   they can try in the simulator (see the root `README.md`).
2. **Offer to integrate the library into their project.** Ask for the path to
   their MPLAB X project if you don't have it yet.
3. If they accept, run the **Integration Interview** (§3) *before* editing
   anything. Do not copy files or change build settings until the mode and
   target are decided.

Keep it short and concrete — the user is an embedded developer.

---

## 3. Integration Interview (ask before integrating)

Ask these questions (use the question tool when available). **Question 1 is the
most important** and decides the whole integration shape.

### Q1 — Which capture mode? (REQUIRED)

- **A) Debugger / breakpoint mode** — *no persistent RAM, no reset.*
  On an exception the handler captures the dump into a RAM buffer and then
  **halts in a loop / software breakpoint**. The developer, with the debugger
  attached, reads the dump **string `edm_dump.msg` in a Variable/Watch window**.
  Simplest: no UART, no linker changes, no reset. Best for bench debugging.

- **B) Persistent-RAM + UART mode** — *store-then-reboot-then-print.*
  On an exception the dump is written to **persistent (no-init) RAM**, the device
  does a **software reset**, and on the **next boot** the application prints the
  stored dump over a **UART / console**. Best for field units with no debugger
  attached. Requires: an output callback, a persistent/no-init section, and a
  startup call.

### Q2 — Target architecture?

PIC32MZ / PIC32MZW1 (WFI32) / PIC32MX / PIC32MK / **PIC32MM**.
Usually auto-detected from XC32 device macros, but confirm — it changes defaults
(see §6). **PIC32MM** in particular defaults to CP0-only capture, a small
buffer, and the microMIPS flag (full GPR capture also works on PIC32MM — see §5).

### Q3 — (Mode B only) Output transport?

How should the stored dump be printed on reboot? e.g. Harmony `SYS_CONSOLE_PRINT`,
a bare UART `Ux TXREG` poll loop, `printf`/stdio, or a USB-CDC console. This
becomes the output callback. (The bundled example uses `printf`/stdio → UART1.)

### Q4 — Does the project already have an exception handler / vector?

Critical to avoid duplicate symbols. Check whether the project already defines
`_general_exception_handler` (e.g. a framework-generated exception file) and
whether it already generates the assembly vector `_general_exception_context`.
See §5 for how to resolve conflicts.

### Q5 — Create a disassembly-listing batch job? (offer this)

Ask the user: **"Shall I create a Windows batch job for your project that
generates a disassembly listing from your ELF, so you can locate an exception by
its address?"**

If yes, adapt the templates in `MIPS/Exception_Dump_Mips/tools/` (see §11) to the
user's XC32 version and ELF path. Whether or not they want the batch, point them
to
[`MIPS/Exception_Dump_Mips/tools/reading_the_dump.md`](MIPS/Exception_Dump_Mips/tools/reading_the_dump.md),
which explains how to turn a dump (`epc`, `ra`, `badvaddr`, cause) into an exact
source line.

### Q6 — Create a Python dump analyzer? (offer this)

Ask the user: **"Shall I write a Python program that reads the UART dump from
your clipboard and analyses the exception — what happened, why, and where?"**

If yes, adapt
[`MIPS/Exception_Dump_Mips/tools/analyze_dump.py`](MIPS/Exception_Dump_Mips/tools/analyze_dump.py)
to their project (see §11): it reads the dump from the clipboard (or a file),
**generates the disassembly listing itself** (`xc32-objdump`), decodes the cause,
applies WHY heuristics (NULL / near-NULL pointer, misalignment, bad function
pointer, PC corruption…), does a full register evaluation, and resolves
`epc`/`ra` to functions + source `file:line` via `xc32-addr2line`. The
`pic32mm_app.X/analyze_dump.py` in this repo is a ready reference of the adapted
result.

---

## 4. Integration steps

> Reminder: `include/…`, `src/…`, `examples/…` below are under
> `MIPS/Exception_Dump_Mips/`.

### How you (Claude) wire the files into the user's MPLAB X project

You can perform the file/build registration **for the user** by editing their
project's `<their-project>.X/nbproject/configurations.xml` directly — no manual
clicking in the IDE — so the library shows up in the **MPLAB X project tree** and
is picked up by the **build**. Edit three things:

- **Source files (compiled + linked):** add an `<itemPath>` for each library
  source (`…/src/exception_dump_mips.c`, `…/src/exception_dump_mips_port.c` for
  Mode B, and `…/src/exception_dump_mips_vector.S` only if the project has no
  vector) inside `<logicalFolder name="SourceFiles">`. Group them in a sub-folder,
  e.g. `<logicalFolder name="Exception_Dump_Mips">`.
- **Header files (tree + indexing):** add an `<itemPath>` for each library header
  (`exception_dump_mips.h`, `…_config.h`, `…_target.h`, `…_port.h`) inside
  `<logicalFolder name="HeaderFiles">`. This is display/indexing only — not
  required to build, but expected for a complete project tree.
- **Include path:** append the library `include/` folder to the
  `extra-include-directories` property in **both** the `C32` **and** `C32CPP`
  blocks (semicolon-separated). This is what actually lets the compiler find the
  headers.

Use paths **relative to the `.X` folder** (e.g. `../MIPS/Exception_Dump_Mips/...`
if the library sits alongside, or wherever the user placed it). MPLAB X
**regenerates the `Makefile-*.mk` from `configurations.xml`** when the project is
next opened/built, so these edits are the durable way to wire the files in —
equivalent to *Add Existing Item…* + *Project Properties → xc32-gcc → Include
directories* in the GUI. After editing, **tell the user to reopen/build in MPLAB
X** so the makefiles regenerate, and confirm the files appear in the tree.

The bundled example's `pic32mm_app.X/nbproject/configurations.xml` is a
worked reference of exactly these three edits.

### Common to both modes
1. Add the include path to `MIPS/Exception_Dump_Mips/include/`.
2. Add `MIPS/Exception_Dump_Mips/src/exception_dump_mips.c` to the MPLAB X project.
3. Confirm the target (Q2). Auto-detected from `<xc.h>`; force with
   `-DEDM_TARGET_PIC32MM` (etc.) if needed. See
   `MIPS/Exception_Dump_Mips/include/exception_dump_mips_target.h`.
4. Resolve the handler-symbol conflict (Q4 / §5).

### Mode A — Debugger / breakpoint
5. Set `-DEDM_AUTO_RESET_AFTER_CAPTURE=0` (handler halts instead of resetting).
6. Persistent RAM is **not** required. To keep the buffer in normal RAM, define
   `EDM_PERSIST_ATTR` to nothing (e.g. `-DEDM_PERSIST_ATTR=`). You may also leave
   it persistent — harmless.
7. GPR capture: on PIC32MZ/MX the default gives full GPRs; you need the assembly
   vector present (project's own, or add
   `MIPS/Exception_Dump_Mips/src/exception_dump_mips_vector.S` if the project has
   none). On PIC32MM the default is CP0-only, but full GPR capture also works —
   add the vector `.S` and build with `-DEDM_CAPTURE_GPR=1` (the vector passes the
   frame base to the handler in `a2`; verified on microMIPS). Leave
   `EDM_CAPTURE_GPR=0` if you only need the CP0 registers.
8. Build, flash with debugger, provoke a crash. When it halts, add
   **`edm_dump.msg`** (and optionally `edm_excep_code` / `edm_excep_addr`) to a
   Watch window and read the decoded dump. No output callback needed.
   - Tip: to force a clean stop, the developer can set a breakpoint on the final
     `while (1)` loop in `_general_exception_handler`, or you can suggest adding
     `__builtin_software_breakpoint();` there under `#ifdef __DEBUG`.

### Mode B — Persistent-RAM + UART
5. Keep `EDM_AUTO_RESET_AFTER_CAPTURE=1` (default) so the handler stores + resets.
6. Ensure the persistent/no-init section exists:
   - **XC32 (recommended):** default `EDM_PERSIST_ATTR = __attribute__((persistent))`
     works with the stock linker script (`.persist`). Nothing to do.
   - **Custom section:** `-DEDM_PERSIST_ATTR=__attribute__((section(".exception_dump_noinit")))`
     plus a `NOLOAD` section in the `.ld` (snippet in
     `MIPS/Exception_Dump_Mips/examples/baremetal_pic32_example/integration_notes.md`).
7. Add `MIPS/Exception_Dump_Mips/src/exception_dump_mips_port.c` (weak default
   reset = SYSKEY/RSWRST), or register a custom reset with
   `exception_dump_mips_set_reset_callback()`.
8. Early in startup, once the console/UART is ready:
   ```c
   exception_dump_mips_init();
   exception_dump_mips_set_output_callback(my_putc, my_puts /* or NULL */);
   exception_dump_mips_check_and_print_previous();   /* prints + clears once */
   ```
   Wire `my_putc`/`my_puts` to the transport from Q3 (see
   `MIPS/Exception_Dump_Mips/examples/`, and `src/main.c` in this repo for a
   printf/stdio wiring).
9. Build, provoke a crash, confirm the device resets and prints the dump once on
   reboot; a second clean reset prints nothing.

---

## 5. Handler / vector conflict resolution (read before building)

There must be **exactly one** `_general_exception_handler` and **one**
`_general_exception_context` in the whole build.

- **If the project already defines `_general_exception_handler`** (e.g. a
  framework-generated exception file): either
  - exclude/remove that file so this library's handler is the only one, **or**
  - set `-DEDM_INSTALL_GENERAL_HANDLER=0` and keep the project's handler, using
    only this library's persistent-storage + print helpers (you then lose the
    automatic register capture unless you call the capture logic yourself).
- **The assembly vector `MIPS/Exception_Dump_Mips/src/exception_dump_mips_vector.S`:**
  - **Harmony / MCC / Melody projects usually already generate it** → do **not**
    add the `.S`, use theirs.
  - **Bare-metal projects** with no vector → add the `.S`.
  - **Frame passing:** the vector passes the base of the saved register frame to
    the C handler as its **3rd argument (`a2`)**; the handler signature is
    `_general_exception_handler(cause, status, uint32_t *frame)`. This makes GPR
    recovery robust against the handler's own compiler prologue — it does **not**
    read `$sp` live (an earlier version did, and `-O1`/`-O2` prologues corrupted
    the whole dump). The offsets still assume a **140-byte frame** with the
    classic layout; if the installed vector differs, update
    `EDM_STACK_FRAME_BYTES` + the `*_IDX` macros, or set `-DEDM_CAPTURE_GPR=0`
    (CP0-only, always safe).
  - **PIC32MM:** the shipped `.S` assembles as microMIPS and full GPR capture is
    **verified working** (`-DEDM_CAPTURE_GPR=1` + add the `.S`) — this is what the
    bundled example does. Confirm at runtime with the analyzer: for an aligned
    `lw`/`sw` fault, base+offset must equal `badvaddr` (the analyzer prints a
    MISMATCH otherwise).

---

## 6. Configuration reference

Two headers, layered. Precedence: **your `-D` overrides → target defaults →
generic fallbacks.**

- `MIPS/Exception_Dump_Mips/include/exception_dump_mips_target.h` — **select the
  target architecture.** Auto-detects from
  `__PIC32MM__` / `__PIC32MZ__` / `__PIC32MX__` / `__PIC32MK__`, or force with
  `EDM_TARGET_*`. Sets per-family defaults:

  | Target | `EDM_CAPTURE_GPR` | `EDM_DUMP_BUFFER_SIZE` | `EDM_ARCH_MICROMIPS` |
  |--------|:-----------------:|:----------------------:|:--------------------:|
  | PIC32MZ / MZW1 | 1 | 4096 | 0 |
  | PIC32MX | 1 | 1024 | 0 |
  | PIC32MK | 1 | 2048 | 0 |
  | **PIC32MM** | **0** | **512** | **1** |
  | GENERIC | 0 | 512 | 0 |

  > **PIC32MM GPR capture:** the default is CP0-only for safety, but full GPR
  > capture **works reliably on PIC32MM** — add the vector `.S` and build with
  > `-DEDM_CAPTURE_GPR=1`. The vector passes the frame base to the handler in
  > `a2`, so capture no longer depends on the handler's prologue.

- `MIPS/Exception_Dump_Mips/include/exception_dump_mips_config.h` — generic
  options: `EDM_MAGIC_CODE`, `EDM_PERSIST_ATTR`, `EDM_INSTALL_GENERAL_HANDLER`,
  `EDM_AUTO_RESET_AFTER_CAPTURE`, `EDM_STACK_FRAME_BYTES`, `EDM_CAPTURE_GPR`,
  `EDM_CAPTURE_EXTRA_CP0` (Status+BadVAddr), `EDM_INSTALL_TLB_REFILL_LIVE_UART`.

### Key macros by mode
| Macro | Mode A (debugger) | Mode B (persistent+UART) |
|-------|-------------------|--------------------------|
| `EDM_AUTO_RESET_AFTER_CAPTURE` | **0** | 1 (default) |
| `EDM_PERSIST_ATTR` | optional / none | `__attribute__((persistent))` or custom no-init |
| output callback | not needed | required |
| `exception_dump_mips_check_and_print_previous()` | not needed | call on boot |

---

## 7. Public API (in `MIPS/Exception_Dump_Mips/include/exception_dump_mips.h`)

```c
void  exception_dump_mips_init(void);
void  exception_dump_mips_set_output_callback(edm_char_output_fn, edm_string_output_fn);
void  exception_dump_mips_set_reset_callback(edm_reset_fn);
bool  exception_dump_mips_check_and_print_previous(void); /* Mode B: print+clear on boot */
bool  exception_dump_mips_print_current(void);            /* print without clearing       */
void  exception_dump_mips_clear_saved_dump(void);
const char *exception_dump_mips_get_saved_text(void);     /* NULL if no valid dump         */
```
The captured text lives in the global `exception_dump_mips_t edm_dump;`
(`edm_dump.msg` = the string, `edm_dump.magic` = valid flag). In Mode A this is
the variable to watch.

---

## 8. File map

**Example app (repo root):**

| Path | Purpose |
|------|---------|
| `README.md` | Full documentation: library docs + this runnable example. |
| `src/main.c` | Init + Mode B wiring (printf/stdio) + `foo_ex()` crash demo. |
| `pic32mm_app.X/` | MPLAB X project (device PIC32MM0256GPM064, XC32). |
| `pic32mm_app.X/analyze_dump.py` | Project-specific dump analyzer (reference for Q6). |
| `pic32mm_app.X/create_listing.bat` | Project-specific listing generator (reference for Q5). |

**Library (`MIPS/Exception_Dump_Mips/`):**

| Path | Purpose |
|------|---------|
| `include/exception_dump_mips.h` | Public API, `exception_dump_mips_t`, callback typedefs. |
| `include/exception_dump_mips_config.h` | Generic compile-time options; includes the target header. |
| `include/exception_dump_mips_target.h` | **Target-architecture selection** + per-family defaults. |
| `include/exception_dump_mips_port.h` | Weak port hooks (reset, timestamp). |
| `src/exception_dump_mips.c` | Core: `_general_exception_handler`, capture, format, print/clear. |
| `src/exception_dump_mips_port.c` | Weak default reset (SYSKEY/RSWRST) + optional live-UART TLB example. |
| `src/exception_dump_mips_vector.S` | MIPS general-exception vector (add only if the project has none). |
| `examples/harmony3_pic32mz_example/` | Wiring to Harmony `SYS_CONSOLE` (Mode B). |
| `examples/baremetal_pic32_example/` | Bare UART + linker no-init snippet (Mode B). |
| `tools/create_listing.bat` | Windows batch **template**: ELF → disassembly listing + symbols. |
| `tools/analyze_dump.py` | Python **template**: clipboard dump → WHAT/WHY/WHERE. Self-generates the listing (`xc32-objdump`), resolves `epc`/`ra` to function + file:line (`xc32-addr2line`), full register evaluation, detects unaligned `swl/swr`, flags an ELF/dump build mismatch. |
| `tools/reading_the_dump.md` | How to locate an exception (`epc`/`ra`/`badvaddr`) in the listing (manual method). |

---

## 9. Runtime flow (Mode B)

```
exception -> asm vector saves GPR frame, passes its base in a2
          -> _general_exception_handler(cause, status, uint32_t *frame)
  -> read GPRs from *frame + CP0 Cause/EPC/Status/BadVAddr
  -> sprintf into persistent edm_dump.msg, set edm_dump.magic
  -> software reset
== reboot, .persist RAM retained ==
  -> startup calls exception_dump_mips_check_and_print_previous()
  -> magic matches -> print via output callback -> clear magic
```
In **Mode A** the flow stops at "software reset": instead the handler halts and
the developer reads `edm_dump.msg` in the debugger.

---

## 10. Verification checklist (state honestly what was/wasn't verified)

- [ ] Exactly one `_general_exception_handler` in the link (no duplicate symbol).
- [ ] Vector present exactly once; GPR frame layout matches or `EDM_CAPTURE_GPR=0`.
- [ ] Mode B: persistent section exists and is not cleared at startup.
- [ ] Mode B: output callback registered before `check_and_print_previous()`.
- [ ] Persistence only survives a **warm/software reset**, not power-on/BOR.
- [ ] PIC32MM: microMIPS build; GPR capture works (add the vector `.S` +
      `-DEDM_CAPTURE_GPR=1`). Confirm at runtime that base+offset == `badvaddr`
      for an aligned fault (the analyzer's MISMATCH check).
- [ ] Test with a forced crash (misaligned store / null-fn-ptr) in a safe build.

Always tell the user which steps you actually performed vs. which they must
verify in MPLAB X on real hardware.

---

## 11. Disassembly listing tooling (offer per Q5)

To translate a captured dump into an exact source location, the user needs a
disassembly listing of their firmware. The `MIPS/Exception_Dump_Mips/tools/`
folder holds templates:

- `tools/create_listing.bat` — runs `xc32-objdump -d -S -l` and
  `xc32-readelf -s` on the project ELF, producing `*.disassembly.txt` and
  `*.symbols.txt`. It has two `ADAPT` lines: the **XC32 bin path** (version
  differs per install, e.g. `...\xc32\v4.60\bin`) and the **ELF path**
  (`<project>.X\dist\<config>\production\<name>.production.elf`).
- `tools/analyze_dump.py` — reads a dump from the **clipboard** (or a file) and
  prints WHAT / WHY / WHERE. It **auto-locates the ELF and generates the
  disassembly listing itself** (`xc32-objdump`), so `create_listing.bat` is
  optional. It decodes the cause, applies heuristics (NULL / near-NULL pointer,
  misaligned/unaligned `swl/swr` access, bad function pointer, corrupted PC,
  which register held the bad pointer), does a **full register evaluation**
  (classifies every GPR: code/RAM/NULL/near-NULL/== badvaddr), and resolves
  `epc`/`ra` to **function + source file path + line** via `xc32-addr2line` —
  giving both the faulting function and its caller. For aligned `lw`/`sw` it
  cross-checks base+offset against `badvaddr` and prints a **MISMATCH** when the
  ELF/listing does not match the dump's build. Standard Python 3, no third-party
  packages (clipboard via pyperclip → tkinter → PowerShell fallback).
- `tools/reading_the_dump.md` — the manual read-the-dump guide (`epc` → faulting
  instruction, `ra` → caller, `badvaddr` → bad pointer, KSEG-nibble and
  branch-delay-slot caveats, a worked example).

**When the user accepts the Q5 offer:**
1. Find their XC32 version (look under `C:\Program Files\Microchip\xc32\`) and
   their ELF output path (from the `.X` project / build config).
2. Produce a project-specific copy of `create_listing.bat` with those two paths
   filled in (place it in their `.X` folder, or wherever they prefer). Either
   edit the template in place for them or hand them an adapted copy — don't leave
   the placeholder paths. `pic32mm_app.X/create_listing.bat` is a filled-in
   reference.
3. Offer to run it once (if a build ELF exists) and, if a real dump is available,
   walk them through `reading_the_dump.md` against their listing.

**If the user declines the batch**, still point them at
`MIPS/Exception_Dump_Mips/tools/reading_the_dump.md` — the method works with any
disassembly they already have (e.g. MPLAB X → Window → Target Memory Views →
Disassembly, or an existing `.lst`).

**When the user accepts the Q6 offer (Python analyzer):**
1. Copy `tools/analyze_dump.py` into a project-specific version (usually into the
   `.X` folder). Set `XC32_BIN` to their XC32 version's `bin` dir and the
   `ELF_CANDIDATES` list to their `dist/<config>/{production,debug}/*.elf` paths.
   The tool **generates the listing itself** (`xc32-objdump`) and resolves
   addresses with `xc32-addr2line`, so a separate `DEFAULT_LISTING` (Q5) is not
   required — Q5's `create_listing.bat` is now optional/complementary.
   `pic32mm_app.X/analyze_dump.py` is a filled-in reference.
2. If they changed the dump text format, adjust the regexes in `parse_dump()` and
   the register names accordingly.
3. Confirm their Python (3.x) is available; the analyzer needs no extra packages.
   Demonstrate it: `python analyze_dump.py` reading a real dump from the
   clipboard (Ctrl+A / Ctrl+C in the MPLAB X Value window), or a pasted file.
4. Explain the workflow: copy the UART/Watch dump → run the script → read
   WHAT / WHY / WHERE. The WHERE section resolves `epc`/`ra` to function +
   file:line automatically (faulting function **and** its caller). Build a DEBUG
   image so the ELF carries the DWARF line info addr2line needs.
5. Tell the user the big payoff with Claude Code: the analyzer builds a detailed
   crash context (cause, faulting instruction, registers, `file:line`), so they
   can hand it straight to Claude and continue root-causing in the same session.

Keep these templates generic; only the per-project copy gets real paths.
