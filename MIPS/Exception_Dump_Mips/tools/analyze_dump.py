#!/usr/bin/env python3
"""
analyze_dump.py  -  TEMPLATE

Reads a PIC32 / MIPS exception dump (as produced by the Exception Dump library)
from the CLIPBOARD (or a file) and prints an analysis:

    WHAT happened  -  the decoded exception cause
    WHY  it happened - heuristics (NULL pointer, misalignment, bad fn pointer...)
    WHERE it happened - epc / ra addresses, resolved against a disassembly
                        listing (from tools/create_listing.bat) if available.

Usage
-----
    python analyze_dump.py                              # dump from clipboard
    python analyze_dump.py dump.txt                     # dump from a file
    python analyze_dump.py --listing fw.disassembly.txt # resolve addresses
    python analyze_dump.py dump.txt --listing fw.disassembly.txt

This is a TEMPLATE. Adapt:
  * DEFAULT_LISTING       -> your project's *.disassembly.txt (optional)
  * the regexes in parse_dump() if you changed the dump text format.

No third-party packages are required (clipboard via pyperclip -> tkinter ->
PowerShell fallback). Standard Python 3 only.
"""

import os
import re
import sys

# --- ADAPT: default disassembly listing (create_listing.bat output), or None -
DEFAULT_LISTING = None
# e.g.
# DEFAULT_LISTING = r"C:\proj\MyProject.X\dist\default\production\MyProject.X.production.disassembly.txt"

# CP0 Cause.ExcCode -> text
CAUSE = {
    0:  "Interrupt",
    4:  "Load/fetch address error (AdEL)",
    5:  "Store address error (AdES)",
    6:  "Instruction bus error (IBE)",
    7:  "Data bus error (DBE)",
    8:  "Syscall",
    9:  "Breakpoint",
    10: "Reserved instruction (RI)",
    11: "Coprocessor unusable (CpU)",
    12: "Arithmetic overflow",
    13: "Trap",
}

# Physical-address mask: maps KSEG0 (0x8/0x9...) and KSEG1 (0xA/0xB...) aliases
# of the same physical memory to one value, so listing lookup is nibble-agnostic.
PHYS_MASK = 0x1FFFFFFF

ADDR_LINE = re.compile(r"^([0-9a-fA-F]+):")
FUNC_LINE = re.compile(r"^([0-9a-fA-F]+)\s+<([^>]+)>:")
SKIP_REGS = {"epc"}


# ---------------------------------------------------------------------------
# Clipboard / input
# ---------------------------------------------------------------------------
def get_clipboard():
    try:
        import pyperclip
        return pyperclip.paste()
    except Exception:
        pass
    try:
        import tkinter
        root = tkinter.Tk()
        root.withdraw()
        try:
            return root.clipboard_get()
        finally:
            root.destroy()
    except Exception:
        pass
    try:
        import subprocess
        out = subprocess.run(
            ["powershell", "-NoProfile", "-Command", "Get-Clipboard -Raw"],
            capture_output=True, text=True, timeout=10)
        if out.returncode == 0:
            return out.stdout
    except Exception:
        pass
    return None


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------
def parse_dump(text):
    d = {"regs": {}}

    m = re.search(r"cause\s*=\s*(\d+)", text)
    if m:
        d["cause"] = int(m.group(1))
    m = re.search(r"epc\s*=\s*([0-9a-fA-F]+)", text)
    if m:
        d["epc"] = int(m.group(1), 16)
    m = re.search(r"badvaddr\s*=\s*([0-9a-fA-F]+)", text, re.I)
    if m:
        d["badvaddr"] = int(m.group(1), 16)
    m = re.search(r"status\s*=\s*([0-9a-fA-F]+)", text, re.I)
    if m:
        d["status"] = int(m.group(1), 16)
    # also accept "addr=" (older format uses addr= for epc)
    if "epc" not in d:
        m = re.search(r"addr\s*=\s*([0-9a-fA-F]+)", text, re.I)
        if m:
            d["epc"] = int(m.group(1), 16)

    # capture every  <reg>=<8 hex>  pair (v0, a0, ra, sp, ...)
    for name, val in re.findall(r"\b([a-z][a-z0-9]{1,2})\s*=\s*([0-9a-fA-F]{8})\b", text):
        if name in SKIP_REGS:
            continue
        d["regs"][name] = int(val, 16)

    return d


# ---------------------------------------------------------------------------
# Address helpers
# ---------------------------------------------------------------------------
def is_code_addr(a):
    # PIC32 program/boot flash appears as 0x9Dxxxxxx/0xBDxxxxxx (program) or
    # 0x9FCxxxxx/0xBFCxxxxx (boot); physical 0x1D.. / 0x1FC..
    p = a & PHYS_MASK
    return (0x1D000000 <= p <= 0x1D7FFFFF) or (0x1FC00000 <= p <= 0x1FCFFFFF)


def is_ram_addr(a):
    p = a & PHYS_MASK
    return 0x00000000 <= p <= 0x0007FFFF  # up to 512 KB RAM window


def resolve(listing, addr):
    """Find addr in an objdump -S listing. Returns dict or None."""
    if not listing:
        return None
    target = addr & PHYS_MASK
    hit = None
    for i, raw in enumerate(listing):
        s = raw.strip()
        m = ADDR_LINE.match(s)
        if m and (int(m.group(1), 16) & PHYS_MASK) == target:
            hit = i
            break
    if hit is None:
        return None

    func = None
    for j in range(hit, -1, -1):
        m = FUNC_LINE.match(listing[j].strip())
        if m:
            func = m.group(2)
            break

    src = None
    for j in range(hit - 1, max(hit - 8, -1), -1):
        line = listing[j].rstrip("\n")
        st = line.strip()
        if not st:
            continue
        if ADDR_LINE.match(st) or FUNC_LINE.match(st):
            break
        src = st  # objdump -S prints the C source line here
        break

    return {"func": func, "instr": listing[hit].rstrip("\n"), "src": src}


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------
def analyze(d, listing):
    what, why, where = [], [], []

    cause = d.get("cause")
    epc = d.get("epc")
    bad = d.get("badvaddr")
    regs = d["regs"]

    ctext = CAUSE.get(cause, "Unknown/Reserved (code=%s)" % cause)
    what.append("Exception: %s" % ctext)
    if epc is not None:
        what.append("Faulting PC (epc) = 0x%08x" % epc)
    if bad is not None and cause in (4, 5, 6, 7):
        what.append("Bad address (badvaddr) = 0x%08x" % bad)
    if "status" in d:
        what.append("Status = 0x%08x" % d["status"])

    # ---- WHY -------------------------------------------------------------
    if cause in (4, 5):
        rw = "read" if cause == 4 else "write"
        if bad == 0:
            why.append("NULL-pointer %s: badvaddr is 0x00000000 -> a pointer used "
                       "for a %s was NULL/uninitialised." % (rw, rw))
        elif bad is not None and (bad & 0x3):
            why.append("Misaligned %s: badvaddr 0x%08x is not 4-byte aligned -> a "
                       "word %s needs a 32-bit-aligned address (bad cast / pointer "
                       "arithmetic)." % (rw, bad, rw))
        elif cause == 4 and bad is not None and epc is not None and (bad & PHYS_MASK) == (epc & PHYS_MASK):
            why.append("Instruction fetch from a bad address: the CPU tried to "
                       "execute 0x%08x -> corrupted return address / bad function "
                       "pointer." % bad)
        elif bad is not None:
            why.append("%s to an invalid/unmapped address 0x%08x." %
                       ("Read" if cause == 4 else "Write", bad))
        # which register held the offending pointer? (only useful for a
        # non-zero address; for NULL almost every register would match)
        if bad:
            hits = sorted(r for r, v in regs.items() if v == bad)
            if hits:
                why.append("The bad address is also in register(s) %s -> that "
                           "register most likely held the offending pointer." %
                           ", ".join(hits))
    elif cause == 6:
        why.append("Instruction bus error: instruction fetch from a non-existent "
                   "code address -> jump through a bad/uninitialised function "
                   "pointer, or a corrupted return address (check ra).")
    elif cause == 7:
        why.append("Data bus error: access to a non-existent peripheral/RAM "
                   "address, or a stray/DMA pointer.")
    elif cause == 10:
        why.append("Reserved instruction: the CPU decoded non-code as an "
                   "instruction -> PC ran into data / an overwritten function, or "
                   "a bad function pointer.")
    elif cause == 11:
        why.append("Coprocessor unusable: e.g. a floating-point instruction ran "
                   "while the FPU (CP1) is disabled.")
    elif cause == 12:
        why.append("Arithmetic overflow in a trapping add/sub.")
    elif cause == 13:
        why.append("Trap fired (e.g. a runtime check such as divide-by-zero).")
    elif cause == 9:
        why.append("Breakpoint instruction executed.")
    elif cause is not None:
        why.append("No specific heuristic for cause=%s; inspect epc/ra in the "
                   "listing." % cause)
    else:
        why.append("Could not read a cause code from the dump.")

    if epc is not None and not is_code_addr(epc):
        extra = " (it points into RAM -> executing from RAM / stack)" if is_ram_addr(epc) else ""
        why.append("NOTE: epc 0x%08x is NOT in a normal code region%s -> the "
                   "program counter itself is likely corrupted (runaway pointer / "
                   "stack overflow)." % (epc, extra))

    # ---- WHERE -----------------------------------------------------------
    def describe(label, addr):
        where.append("%s 0x%08x  ->  %s" % (label, addr, "faulting instruction"
                     if label == "epc" else "caller / return address"))
        info = resolve(listing, addr)
        if info:
            fn = info["func"] or "?"
            where.append("      in function: %s" % fn)
            if info["src"]:
                where.append("      source: %s" % info["src"])
            where.append("      %s" % info["instr"].strip())
        elif listing is not None:
            where.append("      (address not found in listing - rebuild the "
                         "listing from the SAME firmware image)")

    if epc is not None:
        describe("epc", epc)
    if "ra" in regs:
        describe("ra ", regs["ra"])

    if listing is None:
        where.append("Tip: run tools/create_listing.bat and re-run with "
                     "--listing <file>.disassembly.txt to resolve these addresses "
                     "to functions and source lines.")

    return what, why, where


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main(argv):
    listing_path = DEFAULT_LISTING
    dump_path = None
    args = argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("--listing", "-l"):
            i += 1
            listing_path = args[i] if i < len(args) else None
        elif a in ("--help", "-h"):
            print(__doc__)
            return 0
        else:
            dump_path = a
        i += 1

    # get the dump text
    if dump_path:
        with open(dump_path, encoding="utf-8", errors="ignore") as f:
            text = f.read()
    else:
        text = get_clipboard()
        if not text:
            print("[ERROR] Could not read the clipboard and no file was given.")
            print("        Pass a file:  python analyze_dump.py dump.txt")
            return 2

    d = parse_dump(text)
    if "cause" not in d and "epc" not in d:
        print("[ERROR] No exception dump recognised in the input.")
        print("        Expected fields like 'cause=', 'epc=', 'badvaddr='.")
        return 2

    listing = None
    if listing_path:
        if os.path.exists(listing_path):
            with open(listing_path, encoding="utf-8", errors="ignore") as f:
                listing = f.readlines()
        else:
            print("[warn] listing not found: %s (continuing without it)\n" % listing_path)

    what, why, where = analyze(d, listing)

    print("=" * 60)
    print(" EXCEPTION ANALYSIS")
    print("=" * 60)
    print("\nWHAT:")
    for x in what:
        print("  - " + x)
    print("\nWHY:")
    for x in why:
        print("  - " + x)
    print("\nWHERE:")
    for x in where:
        print("  " + x if x.startswith("      ") else "  - " + x)
    print("\n" + "=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
