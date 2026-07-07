# tools/ — Disassembly listing helpers

Templates that turn a captured exception dump into an exact source location.

| File | Purpose |
|------|---------|
| [`create_listing.bat`](create_listing.bat) | Windows batch **template**: runs `xc32-objdump`/`xc32-readelf` on your ELF to produce a source-interleaved disassembly listing and a symbol table. Adapt the two `ADAPT` lines (XC32 path, ELF path) to your project. |
| [`analyze_dump.py`](analyze_dump.py) | Python **template**: reads a dump from the **clipboard** (or a file) and prints a WHAT / WHY / WHERE analysis; resolves `epc`/`ra` to functions + source lines when given a listing. Standard Python 3, no extra packages. |
| [`reading_the_dump.md`](reading_the_dump.md) | How to read the dump fields (`epc`, `ra`, `badvaddr`, cause) and find the faulting instruction / call site in the generated listing (the manual method behind the analyzer). |

## Quick start

1. Edit `create_listing.bat` → set `XC32_BIN` (your installed XC32 version) and
   the ELF path. Or pass the ELF as an argument.
2. Run it from your `.X` project folder:
   ```
   create_listing.bat  dist\<config>\production\<name>.production.elf
   ```
3. When an exception is captured, either:
   - **Automated:** copy the UART dump to the clipboard and run
     `python analyze_dump.py --listing <name>.disassembly.txt` for a WHAT / WHY /
     WHERE report, **or**
   - **Manual:** open `reading_the_dump.md` and follow the checklist to locate
     `epc` in `*.disassembly.txt`.

> These are generic templates. In a Claude Code session (see the top-level
> `CLAUDE.md`), Claude can generate versions wired to your exact project paths
> and XC32 version, or you can adapt them by hand.
