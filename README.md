# PIC32 MIPS Exception Dump — Library + Runnable PIC32MM Example

This repository bundles two things:

1. **The PIC32 / MIPS Exception Dump library** — a small, portable C library that
   captures a MIPS/PIC32 CPU exception (crash) dump (CP0 registers + the full GPR
   set) and turns it into a precise, human-readable post-mortem, down to the exact
   source line that caused the fault. It lives in
   [`MIPS/Exception_Dump_Mips/`](MIPS/Exception_Dump_Mips/).

2. **A ready-to-run PIC32MM example project** (this MPLAB X project,
   `pic32mm_app.X` + `src/`) that integrates the library and **deliberately
   crashes** so you can play the whole flow through **in the MPLAB X Simulator** —
   capture, reset, print, and analyze.

## Repository layout

```
pic32mm_app/                         <- this repo (a Harmony 3 PIC32MM project)
├─ pic32mm_app.X/                    <- MPLAB X project
│  ├─ nbproject/                     <- project definition (build config)
│  ├─ analyze_dump.py                <- project-specific dump analyzer (see below)
│  └─ create_listing.bat            <- optional: ELF -> disassembly listing
├─ src/
│  ├─ main.c                         <- init + Mode B wiring + foo_ex() crash demo
│  └─ config/default/                <- Harmony-generated code (UART1, clocks, ...)
└─ MIPS/Exception_Dump_Mips/         <- THE LIBRARY (reusable, framework-independent)
   ├─ include/  src/  tools/  docs/
   ├─ README.md                      <- full library documentation
   └─ CLAUDE.md                      <- guide for Claude Code assisted integration
```

## Play it through in the simulator (didactic example)

1. Open `pic32mm_app.X` in **MPLAB X** (device: PIC32MM0256GPM064, XC32).
2. Build and run on the **Simulator**. `main()` calls `foo_ex()`, which
   deliberately dereferences a near-NULL pointer → a MIPS **Data bus error**.
3. Two ways to see the dump:
   - **Watch (Mode A style):** halt in `_general_exception_handler` and watch
     `edm_dump.msg` — the decoded register dump; `edm_dump.magic` reads
     `0x47114711` when a capture is valid.
   - **UART (Mode B):** the handler stores the dump in persistent RAM and resets;
     on reboot it is printed once over UART1 (Simulator *UART1 IO* window).
4. Copy the dump text and run the analyzer from the `.X` folder:
   ```
   python analyze_dump.py
   ```
   It auto-locates the ELF, generates the disassembly listing, and resolves the
   fault to **function + source file + line** (`foo_ex` in `src/main.c`, called
   from `main`), with a full register evaluation.

See [`MIPS/Exception_Dump_Mips/README.md`](MIPS/Exception_Dump_Mips/README.md) for
the *Worked Example* section that walks through a real capture of this project.

## Use the library in your own project

- **By hand:** follow
  [`MIPS/Exception_Dump_Mips/README.md`](MIPS/Exception_Dump_Mips/README.md) —
  copy `include/` + `src/`, wire an output callback, resolve the handler/vector,
  pick a capture mode.
- **Guided with Claude Code (much faster):** open
  [`MIPS/Exception_Dump_Mips/`](MIPS/Exception_Dump_Mips/) in Claude Code and say
  *"integrate the MIPS exception dump library into my project"* (or *"read
  CLAUDE.md"*). [`CLAUDE.md`](MIPS/Exception_Dump_Mips/CLAUDE.md) drives an
  interview-driven session (capture mode, target device, UART transport, handler
  conflicts) and performs the integration — using **this PIC32MM project as the
  reference template**.

## Notes

- Build output (`pic32mm_app.X/dist`, `build`, generated listings) is **not**
  tracked — see [`.gitignore`](.gitignore). Build the project to regenerate it.
- The example enables full GPR capture on PIC32MM
  (`-DEDM_CAPTURE_GPR=1` + the microMIPS exception vector). The library default
  for PIC32MM is CP0-only; see the library README/CLAUDE.md.
- Persistent-RAM dumps survive only a **warm/software reset**, not power-on/BOR.
