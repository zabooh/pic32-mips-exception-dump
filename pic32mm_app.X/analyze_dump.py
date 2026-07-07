#!/usr/bin/env python3
"""
analyze_dump.py  -  pic32mm_app (PIC32MM0256GPM064)

Full post-mortem analyzer for the MIPS/PIC32 Exception Dump library.

Given an exception dump (CP0 registers + the full GPR register dump) copied from
the UART console, this tool:

  1. Locates the project ELF and GENERATES the disassembly listing itself
     (xc32-objdump -d -S -l) - no separate batch file needed.
  2. Fully parses and EVALUATES the exception and every register in the dump.
  3. Finds the exact code location of the fault in the listing and resolves it
     with xc32-addr2line to  function + SOURCE FILE (full path) + LINE NUMBER.
  4. Explains WHAT the exception was, WHY it happened (the faulting instruction
     and which pointer/register caused it), and WHERE (source file:line).

Usage
-----
    python analyze_dump.py                 # dump from clipboard, auto ELF+listing
    python analyze_dump.py dump.txt        # dump from a file
    python analyze_dump.py --regen         # force-regenerate the listing
    python analyze_dump.py --elf <path>    # use a specific ELF
    python analyze_dump.py --listing <f>   # use a specific listing (skip objdump)

The ELF/listing MUST come from the SAME firmware image that produced the dump,
otherwise the addresses will not match. Build a DEBUG image (it carries the
DWARF line info addr2line needs).

Standard Python 3 only (clipboard via pyperclip -> tkinter -> PowerShell).
"""

import glob
import os
import re
import subprocess
import sys

# ---------------------------------------------------------------------------
# Project configuration  (this script lives in the .X project folder)
# ---------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))

# XC32 toolchain bin dir (this project uses XC32 v4.60).
XC32_BIN = os.environ.get("XC32_BIN", r"C:\Program Files\Microchip\xc32\v4.60\bin")

def _tool(name):
    exe = os.path.join(XC32_BIN, name + (".exe" if os.name == "nt" else ""))
    return exe if os.path.exists(exe) else name  # fall back to PATH

OBJDUMP  = _tool("xc32-objdump")
ADDR2LINE = _tool("xc32-addr2line")

# Where MPLAB X drops the ELF (production preferred, else debug).
ELF_CANDIDATES = [
    os.path.join(HERE, r"dist\default\production\pic32mm_app.X.production.elf"),
    os.path.join(HERE, r"dist\default\debug\pic32mm_app.X.debug.elf"),
]

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

# ABI role of each MIPS o32 register (for the register evaluation section).
REG_ROLE = {
    "v0": "return value / syscall", "v1": "return value",
    "a0": "arg 1", "a1": "arg 2", "a2": "arg 3", "a3": "arg 4",
    "t0": "temp", "t1": "temp", "t2": "temp", "t3": "temp", "t4": "temp",
    "t5": "temp", "t6": "temp", "t7": "temp", "t8": "temp", "t9": "temp/call",
    "s0": "saved", "s1": "saved", "s2": "saved", "s3": "saved", "s4": "saved",
    "s5": "saved", "s6": "saved", "s7": "saved",
    "k0": "kernel", "k1": "kernel",
    "gp": "global ptr", "sp": "stack ptr", "fp": "frame ptr",
    "ra": "return address",
}
REG_ORDER = ["v0", "v1", "a0", "a1", "a2", "a3",
             "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9",
             "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
             "k0", "k1", "gp", "sp", "fp", "ra"]

# Physical-address mask: folds KSEG0/KSEG1 aliases of the same physical memory.
PHYS_MASK = 0x1FFFFFFF
ADDR_LINE = re.compile(r"^([0-9a-fA-F]+):")
FUNC_LINE = re.compile(r"^([0-9a-fA-F]+)\s+<([^>]+)>:")
# memory operand in objdump output, e.g.  "lw a0,3(v0)"  /  "sb v1,-4(sp)"
MEM_OP = re.compile(r"^\s*[0-9a-fA-F]+:\s+[0-9a-fA-F ]+\s+"
                    r"(?P<op>[a-z][a-z0-9.]+)\s+"
                    r"\$?(?P<rt>[a-z0-9]+),\s*"
                    r"(?P<off>-?(?:0x)?[0-9a-fA-F]+)\("
                    r"\$?(?P<base>[a-z0-9]+)\)")
SKIP_REGS = {"epc"}


# ---------------------------------------------------------------------------
# Input
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
        out = subprocess.run(
            ["powershell", "-NoProfile", "-Command", "Get-Clipboard -Raw"],
            capture_output=True, text=True, timeout=10)
        if out.returncode == 0:
            return out.stdout
    except Exception:
        pass
    return None


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
    if "epc" not in d:  # older format used addr= for epc
        m = re.search(r"addr\s*=\s*([0-9a-fA-F]+)", text, re.I)
        if m:
            d["epc"] = int(m.group(1), 16)
    for name, val in re.findall(
            r"\b([a-z][a-z0-9]{1,2})\s*=\s*([0-9a-fA-F]{8})\b", text):
        if name in SKIP_REGS:
            continue
        d["regs"][name] = int(val, 16)
    return d


# ---------------------------------------------------------------------------
# Address classification
# ---------------------------------------------------------------------------
def is_code_addr(a):
    p = a & PHYS_MASK
    return (0x1D000000 <= p <= 0x1D7FFFFF) or (0x1FC00000 <= p <= 0x1FCFFFFF)


def is_ram_addr(a):
    p = a & PHYS_MASK
    return 0x00000000 <= p <= 0x0007FFFF  # up to 512 KB RAM window


def addr_kind(a):
    if a == 0:
        return "NULL"
    if a == 0xFFFFFFFF:
        return "0xFFFFFFFF"
    if a < 0x1000:
        return "near-NULL"
    if is_code_addr(a):
        return "-> code/flash"
    if is_ram_addr(a):
        return "-> RAM"
    return "?"


# ---------------------------------------------------------------------------
# ELF / listing / addr2line
# ---------------------------------------------------------------------------
def find_elf(explicit):
    if explicit:
        return explicit if os.path.exists(explicit) else None
    for c in ELF_CANDIDATES:
        if os.path.exists(c):
            return c
    # last resort: any *.elf under dist/
    hits = glob.glob(os.path.join(HERE, "dist", "**", "*.elf"), recursive=True)
    return hits[0] if hits else None


def ensure_listing(elf, explicit, regen):
    """Return path to a disassembly listing, generating it with objdump if
    needed. This is the 'also generate the listing file' feature."""
    if explicit:
        return explicit if os.path.exists(explicit) else None
    if not elf:
        return None
    listing = elf[:-4] + ".disassembly.txt" if elf.endswith(".elf") \
        else elf + ".disassembly.txt"
    if regen or not os.path.exists(listing) \
            or os.path.getmtime(listing) < os.path.getmtime(elf):
        print("[*] Generating disassembly listing:\n    %s" % listing)
        try:
            with open(listing, "w", encoding="utf-8", errors="ignore") as f:
                r = subprocess.run([OBJDUMP, "-d", "-S", "-l", elf],
                                   stdout=f, stderr=subprocess.PIPE, text=True)
            if r.returncode != 0:
                print("[warn] objdump failed: %s" % (r.stderr or "").strip())
                return None
        except FileNotFoundError:
            print("[warn] xc32-objdump not found (set XC32_BIN). "
                  "Continuing without a listing.")
            return None
    return listing


def addr2line(elf, addr):
    """Resolve addr -> (func, file, line) via xc32-addr2line. Tries the raw
    address and the microMIPS-cleared (bit0) variant."""
    if not elf:
        return None
    for cand in (addr, addr & ~1, addr | 1):
        try:
            r = subprocess.run([ADDR2LINE, "-f", "-e", elf, "0x%08x" % cand],
                               capture_output=True, text=True)
        except FileNotFoundError:
            return None
        if r.returncode != 0:
            continue
        lines = [x.strip() for x in r.stdout.splitlines() if x.strip()]
        if len(lines) >= 2:
            func, fileline = lines[0], lines[1]
            if fileline != "??:?" and not fileline.startswith("??"):
                fl = fileline.rsplit(":", 1)
                path = fl[0]
                line = fl[1].split()[0] if len(fl) > 1 else "?"
                return {"func": func, "file": path, "line": line}
    return None


def listing_lookup(listing_lines, addr):
    """Find addr in the objdump listing; return (instr_text, src_line, func)."""
    if not listing_lines:
        return None
    target = addr & PHYS_MASK
    hit = None
    for i, raw in enumerate(listing_lines):
        m = ADDR_LINE.match(raw.strip())
        if m and (int(m.group(1), 16) & PHYS_MASK) == target:
            hit = i
            break
    if hit is None and (addr & 1):        # microMIPS: retry cleared bit0
        return listing_lookup(listing_lines, addr & ~1)
    if hit is None:
        return None
    func = None
    for j in range(hit, -1, -1):
        m = FUNC_LINE.match(listing_lines[j].strip())
        if m:
            func = m.group(2)
            break
    src = None
    for j in range(hit - 1, max(hit - 8, -1), -1):
        st = listing_lines[j].strip()
        if not st:
            continue
        if ADDR_LINE.match(st) or FUNC_LINE.match(st):
            break
        src = st
        break
    return {"instr": listing_lines[hit].rstrip("\n"), "src": src, "func": func}


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------
def eval_registers(regs, bad):
    """Return printable lines evaluating every register in the dump."""
    out = []
    for name in REG_ORDER:
        if name not in regs:
            continue
        v = regs[name]
        role = REG_ROLE.get(name, "")
        tags = [addr_kind(v)]
        if bad is not None and v == bad:
            tags.append("== badvaddr!")
        if bad is not None and v != 0 and bad is not None and 0 <= (bad - v) < 0x1000:
            tags.append("base of badvaddr (off +0x%x)" % (bad - v))
        tag = "  ".join(t for t in tags if t and t != "?")
        out.append("  %-3s = 0x%08x  [%-11s] %s" % (name, v, role, tag))
    # any registers not in the canonical order
    for name, v in sorted(regs.items()):
        if name not in REG_ORDER:
            out.append("  %-3s = 0x%08x" % (name, v))
    return out


def explain(d, epc_instr):
    """Human explanation of WHY, using cause + badvaddr + the faulting instr."""
    why = []
    cause = d.get("cause")
    epc = d.get("epc")
    bad = d.get("badvaddr")
    regs = d["regs"]

    # decode the faulting memory instruction, if we have it
    base_reg = base_val = eff = None
    if epc_instr:
        m = MEM_OP.match(epc_instr)
        if m:
            op = m.group("op")
            base_reg = m.group("base")
            off = m.group("off")
            offv = int(off, 16) if "0x" in off.lower() else int(off, 10)
            base_val = regs.get(base_reg)
            if base_val is not None:
                eff = (base_val + offv) & 0xFFFFFFFF
            is_store = op[0] == "s"
            kind = "store" if is_store else "load"
            # swl/swr/lwl/lwr (and 64-bit sdl/sdr/ldl/ldr) are the compiler's
            # UNALIGNED-access helpers. For them base+offset is a word boundary,
            # not the faulting byte, so it deliberately does NOT equal badvaddr.
            unaligned = op in ("swl", "swr", "lwl", "lwr",
                               "sdl", "sdr", "ldl", "ldr")
            if base_val is None:
                why.append("Faulting instruction: '%s %s,%s(%s)' -> a %s through "
                           "base register '%s' (value not in dump)."
                           % (op, m.group("rt"), off, base_reg, kind, base_reg))
            elif unaligned:
                why.append("Faulting instruction: '%s %s,%s(%s)' -> an UNALIGNED %s "
                           "helper. Base register '%s' = 0x%08x is the actual "
                           "(mis)aligned pointer the code used; badvaddr 0x%08x lies "
                           "in the word it spans. (base+offset = 0x%08x is only the "
                           "swl/swr word boundary, so it is not expected to equal "
                           "badvaddr.)" % (op, m.group("rt"), off, base_reg, kind,
                           base_reg, base_val, bad if bad is not None else 0,
                           eff if eff is not None else 0))
            else:
                why.append("Faulting instruction: '%s %s,%s(%s)' -> a %s through base "
                           "register '%s' (=0x%08x, effective address 0x%08x)."
                           % (op, m.group("rt"), off, base_reg, kind, base_reg,
                              base_val, eff))
                if eff is not None and bad is not None and eff != bad:
                    why.append("MISMATCH: the effective address 0x%08x computed from "
                               "the listing does NOT equal badvaddr 0x%08x -> the "
                               "ELF/listing is from a DIFFERENT build than this dump. "
                               "Rebuild against the exact firmware image, then re-run."
                               % (eff, bad))

    if cause in (4, 5):
        rw = "read" if cause == 4 else "write"
        if bad == 0:
            why.append("NULL-pointer %s: badvaddr = 0 -> the pointer was "
                       "NULL/uninitialised." % rw)
        elif bad is not None and (bad & 0x3):
            why.append("Misaligned %s: badvaddr 0x%08x is not 4-byte aligned "
                       "(bad cast / pointer arithmetic on a word access)." % (rw, bad))
        elif bad is not None:
            why.append("%s to an invalid/unmapped address 0x%08x." %
                       (rw.capitalize(), bad))
    elif cause == 6:
        why.append("Instruction bus error: the CPU fetched an instruction from a "
                   "non-existent code address -> jump/return through a bad or "
                   "uninitialised function pointer (inspect ra and t9).")
    elif cause == 7:
        why.append("Data bus error: a load/store hit an address that maps to no "
                   "valid memory or peripheral.")
        if bad is not None and bad < 0x1000:
            why.append("badvaddr 0x%08x is very low (near-NULL) -> almost "
                       "certainly a NULL/near-NULL pointer dereferenced at a small "
                       "struct/array offset (base pointer ~0, member offset 0x%x)."
                       % (bad, bad))
    elif cause == 10:
        why.append("Reserved instruction: the PC decoded non-code as an "
                   "instruction -> ran into data / an overwritten function / a bad "
                   "function pointer.")
    elif cause == 11:
        why.append("Coprocessor unusable: e.g. an FPU (CP1) instruction while the "
                   "FPU is disabled.")
    elif cause == 12:
        why.append("Arithmetic overflow in a trapping add/sub.")
    elif cause == 13:
        why.append("Trap (e.g. a runtime check such as divide-by-zero).")
    elif cause == 9:
        why.append("Breakpoint instruction executed.")

    # which register held the offending pointer?
    if bad:
        hits = sorted(r for r, v in regs.items() if v == bad)
        if hits:
            why.append("The bad address is held verbatim in register(s) %s." %
                       ", ".join(hits))
        near = sorted((r, bad - v) for r, v in regs.items()
                      if v != 0 and 0 < (bad - v) < 0x1000)
        if near and not hits:
            r, off = near[0]
            why.append("Register '%s'=0x%08x + 0x%x = badvaddr -> '%s' most likely "
                       "held the base pointer." % (r, regs[r], off, r))

    # sanity flags
    if regs.get("ra", 1) == 0:
        why.append("NOTE: ra = 0 -> the return address is NULL (a leaf that "
                   "faulted, or the call stack/ra was corrupted).")
    if epc is not None and not is_code_addr(epc):
        extra = " (points into RAM -> executing from RAM/stack)" \
            if is_ram_addr(epc) else ""
        why.append("NOTE: epc 0x%08x is NOT in a normal code region%s -> the PC "
                   "itself is likely corrupted (runaway pointer / stack overflow)."
                   % (epc, extra))
    if "gp" in regs and regs["gp"] and not is_ram_addr(regs["gp"]):
        why.append("NOTE: gp = 0x%08x is not in RAM -> the small-data base looks "
                   "corrupted (or the crash happened before gp was set up)."
                   % regs["gp"])
    return why


def where_lines(elf, listing_lines, label, addr):
    lines = ["%s = 0x%08x" % (label, addr)]
    a2l = addr2line(elf, addr)
    lk = listing_lookup(listing_lines, addr)
    if a2l:
        lines.append("      function : %s" % a2l["func"])
        lines.append("      source   : %s:%s" % (a2l["file"], a2l["line"]))
    elif lk and lk["func"]:
        lines.append("      function : %s" % lk["func"])
    if lk:
        if lk["src"]:
            lines.append("      code     : %s" % lk["src"])
        lines.append("      instr    : %s" % lk["instr"].strip())
        return lines, lk["instr"]
    if not a2l and (listing_lines is not None or elf):
        lines.append("      (address not found - rebuild the listing/ELF from the "
                     "SAME firmware image that produced this dump)")
    return lines, None


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main(argv):
    elf_path = None
    listing_path = None
    dump_path = None
    regen = False
    args = argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("--elf",):
            i += 1; elf_path = args[i] if i < len(args) else None
        elif a in ("--listing", "-l"):
            i += 1; listing_path = args[i] if i < len(args) else None
        elif a in ("--regen",):
            regen = True
        elif a in ("--help", "-h"):
            print(__doc__); return 0
        else:
            dump_path = a
        i += 1

    # dump text
    if dump_path:
        with open(dump_path, encoding="utf-8", errors="ignore") as f:
            text = f.read()
    else:
        text = get_clipboard()
        if not text:
            print("[ERROR] No dump on the clipboard and no file given.")
            print("        Usage: python analyze_dump.py [dump.txt]")
            return 2

    d = parse_dump(text)
    if "cause" not in d and "epc" not in d:
        print("[ERROR] No exception dump recognised (need 'cause='/'epc=').")
        return 2

    # ELF + listing (generated here if missing)
    elf = find_elf(elf_path)
    if elf:
        print("[*] ELF     : %s" % elf)
    else:
        print("[warn] No ELF found under dist/ - address resolution disabled.")
    listing_file = ensure_listing(elf, listing_path, regen)
    listing_lines = None
    if listing_file and os.path.exists(listing_file):
        with open(listing_file, encoding="utf-8", errors="ignore") as f:
            listing_lines = f.readlines()

    cause = d.get("cause")
    epc = d.get("epc")
    bad = d.get("badvaddr")

    print("\n" + "=" * 66)
    print(" EXCEPTION ANALYSIS")
    print("=" * 66)

    # ---- WHAT ----
    print("\nWHAT:")
    print("  - Exception : %s" % CAUSE.get(cause, "Unknown/Reserved (code=%s)" % cause))
    if epc is not None:
        print("  - Faulting PC (epc)      = 0x%08x  [%s]" % (epc, addr_kind(epc)))
    if bad is not None:
        print("  - Bad address (badvaddr) = 0x%08x  [%s]" % (bad, addr_kind(bad)))
    if "status" in d:
        print("  - Status                 = 0x%08x" % d["status"])

    # ---- WHERE (resolve epc first so its instruction feeds WHY) ----
    where = []
    epc_instr = None
    if epc is not None:
        block, epc_instr = where_lines(elf, listing_lines, "epc", epc)
        where += block
    if d["regs"].get("ra"):
        where += where_lines(elf, listing_lines, "ra ", d["regs"]["ra"])[0]

    # ---- WHY ----
    print("\nWHY:")
    for x in explain(d, epc_instr):
        print("  - " + x)

    # ---- WHERE ----
    print("\nWHERE:")
    if where:
        for x in where:
            print("  " + x if x.startswith("      ") else "  - " + x)
    else:
        print("  - (no code addresses to resolve)")

    # ---- REGISTERS ----
    if d["regs"]:
        print("\nREGISTER DUMP EVALUATION:")
        for x in eval_registers(d["regs"], bad):
            print(x)

    print("\n" + "=" * 66)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
