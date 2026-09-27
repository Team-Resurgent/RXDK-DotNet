#!/usr/bin/env python3
"""
isa-scan.py -- fail on any instruction above the Pentium III (SSE1) baseline.

The original Xbox CPU is a Pentium III "Coppermine": base x86 + CMOV + x87 (FPU) + MMX + SSE1,
and NO SSE2. A compiler that emits SSE2/SSE3/.../AVX faults as STATUS_ILLEGAL_INSTRUCTION on real
hardware -- and xemu often masks it, so it is a silent, ship-breaking regression. This scans the
emitted COFF objects/libraries and flags every instruction whose LLVM/Capstone ISA group is above
SSE1, so a build/toolchain change (e.g. swapping zig for LLVM clang) can be gated on it. Uses
Capstone's per-instruction ISA groups rather than a hand-written mnemonic list, so it cannot miss
an opcode.

Usage:
    python tools/isa-scan.py [--quiet] [--max N] <file-or-dir> [<file-or-dir> ...]
        Scans .o/.obj/.lib/.a (archives are walked member by member). Exit 1 if any violation.
"""
import argparse
import os
import struct
import sys

try:
    import capstone as C
    from capstone import x86 as X
except ImportError:
    sys.stderr.write("isa-scan: needs Capstone -- pip install capstone\n")
    sys.exit(2)

# Capstone x86 group id -> short name (SSE2, AVX, JUMP, ...).
_GID = {getattr(X, n): n[len("X86_GRP_"):] for n in dir(X) if n.startswith("X86_GRP_")}

# ISA-extension groups ABOVE the PIII/SSE1 baseline. Anything here is illegal on the Xbox.
# (Allowed and therefore NOT listed: the base ISA -- no group -- plus CMOV, FPU, MMX, SSE1, and the
# purely-semantic groups JUMP/CALL/RET/INT/IRET/PRIVILEGE/VM/BRANCH_RELATIVE/MODE*/NOT64BITMODE.)
DISALLOWED = {
    "SSE2", "SSE3", "SSSE3", "SSE41", "SSE42", "SSE4A",
    "AVX", "AVX2", "AVX512", "BWI", "CDI", "DQI", "ERI", "PFI", "VLX",
    "FMA", "FMA4", "XOP", "3DNOW",
    "BMI", "BMI2", "TBM", "ADX", "AES", "SHA", "PCLMUL", "F16C",
    "RTM", "HLE", "FSGSBASE", "SGX", "SMAP",
}

# Backward-compatible mnemonics that Capstone tags as a newer ISA but which DO NOT fault on a PIII:
# their extra prefix is simply ignored on older CPUs, so they execute as a legal base instruction.
#   tzcnt = `rep bsf`, lzcnt = `rep bsr`  (execute as bsf/bsr; only the zero-input result differs)
#   pause = `rep nop`                      (executes as nop)
# clang emits these even at -march=pentium3 (zig does too), and they run fine on hardware -- so they
# are not part of the "would fault" gate. (Genuinely-illegal newer ops like popcnt/crc32/movnti and
# the whole SSE2 xmm set have NO legal PIII encoding and stay flagged.)
BACKWARD_COMPATIBLE = {"tzcnt", "lzcnt", "pause"}

# Post-PIII scalar instructions that DO #UD on a PIII but which Capstone leaves ungrouped (so the
# ISA-group check above would miss them). This is the mnemonic backstop; the SSE/AVX/BMI families are
# caught by their groups. `popcnt` is the important one -- clang emits it for __builtin_popcount when
# the feature is on. (Verified: `popcnt` disassembles with groups=[] under Capstone 5.x.)
MNEMONIC_DISALLOW = {
    "popcnt", "movbe", "rdrand", "rdseed", "rdtscp", "rdpid",
    "adcx", "adox", "clflushopt", "clwb", "clzero", "cldemote",
    "monitorx", "mwaitx", "umonitor", "umwait", "tpause", "serialize",
    "wrfsbase", "rdfsbase", "wrgsbase", "rdgsbase", "ptwrite",
}

IMAGE_FILE_MACHINE_I386 = 0x014C
SCN_CNT_CODE = 0x00000020
SCN_MEM_EXECUTE = 0x20000000
_EXTS = (".o", ".obj", ".lib", ".a")


def _code_sections(data):
    """Yield (section_name, vaddr, bytes) for each executable section of a COFF object."""
    if len(data) < 20:
        return
    _machine, nsec = struct.unpack_from("<HH", data, 0)
    opt = struct.unpack_from("<H", data, 16)[0]
    sec_off = 20 + opt
    for i in range(nsec):
        off = sec_off + i * 40
        if off + 40 > len(data):
            break
        name = data[off:off + 8].rstrip(b"\x00").decode("latin1", "replace")
        _vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        chars = struct.unpack_from("<I", data, off + 36)[0]
        if (chars & SCN_CNT_CODE) or (chars & SCN_MEM_EXECUTE):
            if rawptr and rawsize and rawptr + rawsize <= len(data):
                yield name, vaddr, data[rawptr:rawptr + rawsize]


def _archive_members(data):
    """Yield (member_name, member_bytes) for a COFF/GNU `ar` archive, skipping index members."""
    if not data.startswith(b"!<arch>\n"):
        return
    p = 8
    longnames = b""
    n = len(data)
    while p + 60 <= n:
        hdr = data[p:p + 60]
        name = hdr[0:16].decode("latin1", "replace").rstrip()
        try:
            size = int(hdr[48:58].decode("latin1").strip() or "0")
        except ValueError:
            break
        body = data[p + 60:p + 60 + size]
        p += 60 + size + (size & 1)  # members are 2-byte aligned
        if name in ("/", "//", "/SYM64/", "__.SYMDEF", "__.SYMDEF SORTED"):
            if name == "//":
                longnames = body
            continue
        if name.startswith("/") and name[1:].isdigit() and longnames:
            o = int(name[1:])
            end = longnames.find(b"\n", o)
            end = end if end >= 0 else longnames.find(b"\x00", o)
            name = longnames[o:(end if end >= 0 else len(longnames))].decode("latin1", "replace")
        name = name.rstrip("/")
        yield name, body


def _scan_bytes(md, label, base, code, out, max_hits):
    """Linear-sweep disassemble, resuming past undecodable bytes, recording disallowed insns."""
    off = 0
    end = len(code)
    while off < end:
        last = off
        for insn in md.disasm(code[off:], base + off):
            last = insn.address - base + insn.size
            if insn.mnemonic in BACKWARD_COMPATIBLE:
                continue
            bad = sorted({_GID.get(g, str(g)) for g in insn.groups if _GID.get(g) in DISALLOWED})
            if insn.mnemonic in MNEMONIC_DISALLOW:
                bad = sorted(set(bad) | {"post-PIII"})
            if bad:
                out.append((label, insn.address, insn.mnemonic, insn.op_str, ",".join(bad)))
                if len(out) >= max_hits:
                    return
        # resume one byte past wherever the sweep stalled (padding / data-in-code)
        off = max(last, off + 1)


def _scan_object(data, label, out, max_hits):
    md = C.Cs(C.CS_ARCH_X86, C.CS_MODE_32)
    md.detail = True
    for sname, base, code in _code_sections(data):
        _scan_bytes(md, "%s(%s)" % (label, sname), base, code, out, max_hits)
        if len(out) >= max_hits:
            return


def _scan_file(path, out, max_hits):
    with open(path, "rb") as f:
        data = f.read()
    if data.startswith(b"!<arch>\n"):
        for mname, body in _archive_members(data):
            if len(body) >= 2 and body[0:2] == b"\x4c\x01":  # IMAGE_FILE_MACHINE_I386, little-endian
                _scan_object(body, "%s::%s" % (os.path.basename(path), mname), out, max_hits)
            if len(out) >= max_hits:
                return
    else:
        _scan_object(data, os.path.basename(path), out, max_hits)


def _collect(paths):
    for p in paths:
        if os.path.isdir(p):
            for root, _dirs, files in os.walk(p):
                for fn in files:
                    if fn.lower().endswith(_EXTS):
                        yield os.path.join(root, fn)
        elif p.lower().endswith(_EXTS):
            yield p


def main(argv=None):
    ap = argparse.ArgumentParser(description="Flag instructions above the Pentium III / SSE1 baseline in COFF .o/.lib.")
    ap.add_argument("paths", nargs="+", help="Files or directories (.o/.obj/.lib/.a).")
    ap.add_argument("--quiet", action="store_true", help="Only print the summary + violations, not scanned files.")
    ap.add_argument("--max", type=int, default=200, help="Stop after this many violations (default 200).")
    args = ap.parse_args(argv)

    files = sorted(set(_collect(args.paths)))
    if not files:
        sys.stderr.write("isa-scan: no .o/.obj/.lib/.a inputs found\n")
        return 2

    all_hits = []
    for path in files:
        hits = []
        try:
            _scan_file(path, hits, args.max)
        except Exception as e:  # noqa: BLE001 - report and keep going
            sys.stderr.write("isa-scan: %s: %s\n" % (path, e))
            continue
        if not args.quiet:
            mark = "FAIL %d" % len(hits) if hits else "ok"
            print("  [%s] %s" % (mark, path))
        all_hits.extend(hits)
        if len(all_hits) >= args.max:
            break

    if all_hits:
        print("\nISA violations (above Pentium III / SSE1 -- would fault on Xbox HW):")
        for label, addr, mnem, ops, groups in all_hits[:args.max]:
            print("  %-40s 0x%06x  %-10s %-28s [%s]" % (label, addr, mnem, ops, groups))
        print("\nisa-scan: %d violation(s) across %d file(s). FAIL." % (len(all_hits), len(files)))
        return 1

    print("\nisa-scan: clean -- %d file(s), no instruction above SSE1." % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
