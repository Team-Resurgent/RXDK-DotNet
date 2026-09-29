#!/usr/bin/env python3
"""Generate Rxdk.Kernel from the xboxkrnl headers.

Every kernel export becomes a method on Rxdk.Kernel. String, boolean, and
out-scalar parameters use managed types; the C wrappers build ANSI_STRING /
OBJECT_ATTRIBUTES / IO_STATUS_BLOCK for the call. Handles, IRPs, and other
kernel addresses stay IntPtr. Sockets are libxnet and are not part of this
surface.
"""
import os
import re
import sys

SDK = os.environ.get("RXDK_SDK", r"C:\ProgramData\RXDK\sdk\include\xboxkrnl")
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CS_PATH = os.path.join(ROOT, "src", "Rxdk.Kernel", "Kernel.cs")
C_PATH = os.path.join(ROOT, "pal", "src", "bind", "kernel_bind.c")

STRING_STRUCT = {"POBJECT_STRING", "PANSI_STRING", "PSTRING", "PCANSI_STRING"}
UNI_STRUCT = {"PUNICODE_STRING", "PCUNICODE_STRING"}
CSTRING = {"PCSZ", "PCSTR", "LPCSTR", "PSZ", "PSTR", "LPSTR", "PCH", "LPCH", "PCHAR"}
UCSTRING = {"PWSTR", "PCWSTR", "LPCWSTR", "LPWSTR", "PWCHAR"}
OA = {"POBJECT_ATTRIBUTES"}

SCALARS = {
    "VOID": "void",
    "NTSTATUS": "i32", "LONG": "i32", "INT": "i32", "HRESULT": "i32", "SHORT": "i32",
    "ULONG": "u32", "DWORD": "u32", "UINT": "u32", "SIZE_T": "u32", "ULONG_PTR": "u32",
    "LONG_PTR": "i32", "ACCESS_MASK": "u32", "DEVICE_TYPE": "u32", "PFN_COUNT": "u32",
    "PHYSICAL_ADDRESS": "u32", "DWORD_PTR": "u32",
    "BOOLEAN": "bool", "BOOL": "bool",
    "BYTE": "u8", "UCHAR": "u8", "KIRQL": "u8", "CCHAR": "u8", "CHAR": "u8",
    "USHORT": "u16", "WORD": "u16", "WCHAR": "u16", "CSHORT": "u16",
    "LONGLONG": "i64", "LARGE_INTEGER": "i64",
    "ULONGLONG": "u64", "ULARGE_INTEGER": "u64",
    "PVOID": "ptr", "HANDLE": "ptr", "LPCVOID": "ptr", "LPVOID": "ptr",
}

OUT_SCALAR = {
    "PULONG": "u32", "PDWORD": "u32", "LPDWORD": "u32", "PUINT": "u32", "LPUINT": "u32",
    "PULONG_PTR": "u32", "PSIZE_T": "u32", "PACCESS_MASK": "u32", "PPHYSICAL_ADDRESS": "u32",
    "PKIRQL": "u8", "PUCHAR": "u8", "PBYTE": "u8",
    "PLONG": "i32", "PINT": "i32", "LPINT": "i32", "PNTSTATUS": "i32",
    "PBOOLEAN": "bool", "PBOOL": "bool",
    "PUSHORT": "u16", "PWORD": "u16", "PWCHAR": None,  # PWCHAR is a string, handled earlier
    "PLONGLONG": "i64", "PULONGLONG": "u64",
    "PHANDLE": "ptr",
}

KEYWORDS = {
    "string", "object", "event", "out", "in", "ref", "base", "lock", "params",
    "int", "uint", "long", "bool", "byte", "char", "fixed", "namespace", "class",
    "public", "private", "static", "void", "new", "return", "default", "operator",
    "decimal", "implicit", "explicit", "checked", "unchecked", "as", "is",
}

def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*?$", "", text, flags=re.M)
    text = "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))
    return text

def load_headers():
    chunks = []
    api = os.path.join(SDK, "api")
    for name in sorted(os.listdir(api)):
        if name.endswith(".h"):
            with open(os.path.join(api, name), "r", encoding="utf-8", errors="replace") as f:
                chunks.append(strip_comments(f.read()))
    return "\n".join(chunks)

def split_type(raw):
    t = raw.replace("CONST", " ").replace("const", " ")
    t = t.replace("volatile", " ").replace("__volatile__", " ")
    t = " ".join(t.split())
    stars = 0
    while t.endswith("*"):
        stars += 1
        t = t[:-1].strip()
    return t, stars

def camel(name):
    if not name:
        return "value"
    n = name[0].lower() + name[1:]
    if n in KEYWORDS:
        n = "@" + n
    return n

def parse_params(args):
    args = " ".join(args.split())
    if not args or args == "void":
        return []
    parts = [p.strip() for p in args.split(",") if p.strip()]
    params = []
    for p in parts:
        optional = "OPTIONAL" in p
        p = p.replace("OPTIONAL", " ")
        direction = "in"
        for label, kind in (("IN OUT", "inout"), ("INOUT", "inout"), ("OUT", "out"), ("IN", "in")):
            if p.startswith(label + " ") or p == label:
                direction = kind
                p = p[len(label):].strip()
                break
        p = " ".join(p.split())
        if not p or p == "void":
            continue
        bits = p.split()
        if len(bits) == 1:
            typ, name = bits[0], "value"
        else:
            name = bits[-1]
            if name.startswith("*"):
                typ = " ".join(bits[:-1]) + name
                name = "value"
                while typ.endswith("*") is False and False:
                    pass
                # pointer attached to the name: "*BaseAddress" already split
            typ = " ".join(bits[:-1])
            if name.startswith("*"):
                typ = typ + " " + name
                name = "arg%d" % len(params)
                # rebuild: type includes stars, name was eaten
            name = name.lstrip("*")
            # stars that were glued to the name belong to the type
            star_prefix = bits[-1][:len(bits[-1]) - len(name)] if name else ""
            if star_prefix:
                typ = typ + " " + star_prefix
        name = re.sub(r"[^A-Za-z0-9_]", "", name) or ("arg%d" % len(params))
        params.append({"type": " ".join(typ.split()), "name": name, "dir": direction, "optional": optional})
    return params

def classify(param):
    base, stars = split_type(param["type"])
    direction = param["dir"]
    kind = None
    if base in OA and stars == 0:
        kind = "oa"
    elif base in STRING_STRUCT and stars == 0:
        # IN OUT counted strings are caller buffers the kernel fills.
        kind = "str_out" if direction in ("out", "inout") else "str_in"
    elif base in UNI_STRUCT and stars == 0:
        kind = "uni_out" if direction in ("out", "inout") else "uni_in"
    elif base in CSTRING and stars == 0:
        kind = {"out": "cstr_out", "inout": "cstr_inout"}.get(direction, "cstr_in")
    elif base in UCSTRING and stars == 0:
        kind = {"out": "uni_out", "inout": "uni_inout"}.get(direction, "uni_in")
    elif base in ("LARGE_INTEGER",) and stars == 0:
        kind = "i64"
    elif base in ("ULARGE_INTEGER",) and stars == 0:
        kind = "u64"
    elif base in ("PLARGE_INTEGER",) or (base == "LARGE_INTEGER" and stars >= 1):
        kind = "i64_ref" if direction in ("out", "inout") else "ptr"
    elif base in ("PULARGE_INTEGER",) or (base == "ULARGE_INTEGER" and stars >= 1):
        kind = "u64_ref" if direction in ("out", "inout") else "ptr"
    elif stars >= 1 and base in ("PVOID", "HANDLE", "LPVOID", "LPCVOID"):
        kind = "ptr_ref" if direction == "inout" else ("ptr_out" if direction == "out" else "ptr")
    elif base in OUT_SCALAR and stars == 0 and OUT_SCALAR[base] is not None and direction in ("out", "inout"):
        sk = OUT_SCALAR[base]
        kind = sk + ("_ref" if direction == "inout" else "_out")
    elif base in OUT_SCALAR and stars == 0 and direction == "in":
        # in PHANDLE is still a pointer
        kind = "ptr"
    elif base in SCALARS and stars == 0:
        kind = SCALARS[base]
    elif base in SCALARS and stars == 1 and SCALARS[base] in ("u32", "i32", "u8", "u16", "bool", "i64", "u64"):
        sk = SCALARS[base]
        kind = sk + ("_ref" if direction == "inout" else "_out")
    elif base == "PIO_STATUS_BLOCK" or base == "IO_STATUS_BLOCK":
        kind = "iosb_ref" if direction == "inout" else "iosb"
    elif stars >= 1 and (base.startswith("P") or base.startswith("LP") or base in ("PVOID", "HANDLE", "VOID")):
        # A star on an existing pointer (PVOID*) is a pointer the kernel writes back.
        kind = "ptr_ref" if direction == "inout" else ("ptr_out" if direction == "out" else "ptr")
    elif stars >= 1 or base.startswith("P") or base.startswith("LP"):
        # Pointer typedefs (PIRP, PFILE_OBJECT) are already one pointer. IN OUT
        # describes the object, not an extra level of indirection.
        kind = "ptr"
    else:
        kind = "u32"
        param["warn"] = base
    param["kind"] = kind
    return kind

def classify_return(ret):
    base, stars = split_type(ret)
    if not base or base == "VOID":
        return "void"
    if stars:
        return "ptr"
    if base in SCALARS:
        return SCALARS[base]
    if base in ("LARGE_INTEGER",):
        return "i64"
    if base in ("ULARGE_INTEGER",):
        return "u64"
    if base.startswith("P") or base.startswith("LP"):
        return "ptr"
    return "u32"

def parse_functions(text):
    funcs = []
    i = 0
    while True:
        m = re.search(r"\b(STDCALL|FASTCALL|CDECL)\b", text[i:])
        if not m:
            break
        cc_at = i + m.start()
        cc = m.group(1)
        head = text[max(0, cc_at - 180):cc_at]
        ret = head.split(";")[-1].split("}")[-1]
        ret = re.sub(r"DECLSPEC_NORETURN", " ", ret)
        ret = re.sub(r"__attribute__\s*\(\([^)]*\)\)", " ", ret)
        ret = " ".join(ret.split())
        j = i + m.end()
        while True:
            rest = text[j:].lstrip()
            pad = len(text[j:]) - len(rest)
            if rest.startswith("DECLSPEC_NORETURN"):
                j += pad + len("DECLSPEC_NORETURN")
                continue
            am = re.match(r"__attribute__\s*\(\([^)]*\)\)", rest)
            if am:
                j += pad + am.end()
                continue
            j += pad
            break
        nm = re.match(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", text[j:])
        if not nm:
            i = cc_at + len(cc)
            continue
        name = nm.group(1)
        paren = j + nm.end() - 1
        depth = 0
        k = paren
        while k < len(text):
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        params = parse_params(text[paren + 1:k])
        for p in params:
            classify(p)
        # A trailing length belongs to an out character buffer, not the caller.
        folded = []
        skip = False
        for idx, p in enumerate(params):
            if skip:
                skip = False
                continue
            if p["kind"] in ("cstr_out", "cstr_inout") and idx + 1 < len(params):
                nxt = params[idx + 1]
                if nxt["kind"] == "u32" and "Length" in nxt["name"]:
                    p["folded_len"] = True
                    skip = True
            folded.append(p)
        funcs.append({
            "name": name,
            "cc": cc,
            "ret": classify_return(ret),
            "ret_raw": ret,
            "params": folded,
            "variadic": "..." in text[paren + 1:k],
        })
        i = k + 1
    # DbgPrint is variadic; one managed string is the usable form.
    for fn in funcs:
        if fn["name"] == "DbgPrint":
            fn["variadic"] = True
            fn["params"] = [{
                "type": "PCSTR", "name": "Format", "dir": "in", "optional": False,
                "kind": "cstr_in",
            }]
        if fn["name"] == "DbgPrompt":
            fn["params"] = [
                {"type": "PCSTR", "name": "Prompt", "dir": "in", "optional": False, "kind": "cstr_in"},
                {"type": "PCH", "name": "Response", "dir": "out", "optional": False, "kind": "cstr_out", "folded_len": True},
            ]
        if fn["name"] in ("RtlSprintf", "RtlSnprintf", "RtlVsprintf", "RtlVsnprintf"):
            fn["variadic"] = True
            fn["sprintf"] = True
            fn["ret"] = "void"
            fn["params"] = [
                {"type": "PCSTR", "name": "Text", "dir": "in", "optional": False, "kind": "cstr_in"},
            ]
    return funcs

def parse_data(text):
    data = []
    for m in re.finditer(r"\bXBAPI\s+([^;]+);", text):
        decl = " ".join(m.group(1).split())
        dm = re.match(r"(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])?\s*$", decl)
        if not dm:
            continue
        data.append({"type": dm.group(1).strip(), "name": dm.group(2), "array": bool(dm.group(3))})
    return data

def c_type(kind):
    return {
        "void": "void", "bool": "int", "i32": "int", "u32": "unsigned int",
        "u8": "unsigned int", "u16": "unsigned int", "i64": "long long", "u64": "unsigned long long",
        "ptr": "void *", "ptr_out": "void **", "ptr_ref": "void **",
        "bool_out": "int *", "bool_ref": "int *",
        "i32_out": "int *", "i32_ref": "int *",
        "u32_out": "unsigned int *", "u32_ref": "unsigned int *",
        "u8_out": "unsigned int *", "u8_ref": "unsigned int *",
        "u16_out": "unsigned int *", "u16_ref": "unsigned int *",
        "i64_out": "long long *", "i64_ref": "long long *",
        "u64_out": "unsigned long long *", "u64_ref": "unsigned long long *",
        "iosb": "IO_STATUS_BLOCK *", "iosb_ref": "IO_STATUS_BLOCK *",
        "str_in": "const char *", "cstr_in": "const char *", "oa": "const char *",
        "uni_in": "const unsigned short *",
        "str_out": None, "str_inout": None, "uni_out": None, "uni_inout": None,
        "cstr_out": None, "cstr_inout": None,
    }[kind]

def cs_public(kind, name):
    t = {
        "bool": "bool", "i32": "int", "u32": "uint", "u8": "byte", "u16": "ushort",
        "i64": "long", "u64": "ulong", "ptr": "IntPtr", "ptr_out": "out IntPtr",
        "ptr_ref": "ref IntPtr", "bool_out": "out bool", "bool_ref": "ref bool",
        "i32_out": "out int", "i32_ref": "ref int", "u32_out": "out uint", "u32_ref": "ref uint",
        "u8_out": "out byte", "u8_ref": "ref byte", "u16_out": "out ushort", "u16_ref": "ref ushort",
        "i64_out": "out long", "i64_ref": "ref long", "u64_out": "out ulong", "u64_ref": "ref ulong",
        "iosb": "out IoStatus", "iosb_ref": "ref IoStatus",
        "str_in": "string", "cstr_in": "string", "oa": "string", "uni_in": "string",
        "str_out": "out string", "uni_out": "out string", "cstr_out": "out string",
        "str_inout": "ref string", "uni_inout": "ref string", "cstr_inout": "ref string",
    }[kind]
    return t + " " + camel(name)

def fail_expr(ret):
    if ret == "void":
        return "return;"
    if ret == "ptr":
        return "return 0;"
    if ret in ("bool", "u32", "u8", "u16", "i64", "u64"):
        return "return 0;"
    return "return (int)0xC000000DL;"

def emit_c(funcs, data):
    lines = []
    a = lines.append
    a("/* Generated by scripts/gen-kernel.py. Do not edit.")
    a(" * Cdecl wrappers around xboxkrnl. Each one builds the kernel strings")
    a(" * for the duration of the call and is the only reference to that import.")
    a(" */")
    a("#include <string.h>")
    a("#include <xboxkrnl/xboxkrnl.h>")
    a("")
    a("typedef void *(*RxdkDlLoad)(const char *name, int flags, char **err, void *ud);")
    a("typedef void *(*RxdkDlSymbol)(void *handle, const char *name, char **err, void *ud);")
    a("typedef void *(*RxdkDlClose)(void *handle, void *ud);")
    a("extern void *mono_dl_fallback_register(RxdkDlLoad, RxdkDlSymbol, RxdkDlClose, void *);")
    a("")
    a("static int rxdk_ansi(ANSI_STRING *s, const char *text)")
    a("{")
    a("    unsigned int n = 0;")
    a("    if (!text)")
    a("        return 0;")
    a("    while (text[n]) {")
    a("        if (n >= 1024)")
    a("            return 0;")
    a("        n++;")
    a("    }")
    a("    s->Length = (USHORT)n;")
    a("    s->MaximumLength = (USHORT)(n + 1);")
    a("    s->Buffer = (PCHAR)text;")
    a("    return 1;")
    a("}")
    a("")
    a("static int rxdk_uni(UNICODE_STRING *s, const unsigned short *text)")
    a("{")
    a("    unsigned int n = 0;")
    a("    if (!text)")
    a("        return 0;")
    a("    while (text[n]) {")
    a("        if (n >= 1024)")
    a("            return 0;")
    a("        n++;")
    a("    }")
    a("    s->Length = (USHORT)(n * 2);")
    a("    s->MaximumLength = (USHORT)((n + 1) * 2);")
    a("    s->Buffer = (PWSTR)text;")
    a("    return 1;")
    a("}")
    a("")
    a("static void rxdk_copy_ansi(char *dst, unsigned int cap, unsigned int *n, ANSI_STRING *s)")
    a("{")
    a("    unsigned int i, len;")
    a("    len = s && s->Buffer ? s->Length : 0;")
    a("    if (n)")
    a("        *n = len;")
    a("    if (!dst || cap == 0)")
    a("        return;")
    a("    if (len >= cap)")
    a("        len = cap - 1;")
    a("    for (i = 0; i < len; i++)")
    a("        dst[i] = s->Buffer[i];")
    a("    dst[len] = 0;")
    a("    if (n)")
    a("        *n = len;")
    a("}")
    a("")
    a("static void rxdk_oa(OBJECT_ATTRIBUTES *oa, ANSI_STRING *name)")
    a("{")
    a("    oa->RootDirectory = NULL;")
    a("    oa->ObjectName = name;")
    a("    oa->Attributes = OBJ_CASE_INSENSITIVE;")
    a("}")
    a("")

    def prep_string(idx, kind, optional, ret):
        """Return (c params, locals, arg expr or None if the arg is the struct pointer)."""
        fail = fail_expr(ret)
        if kind in ("str_in", "oa"):
            cparams = ["const char *a%d" % idx]
            loc = [
                "    ANSI_STRING s%d; ANSI_STRING *p%d = 0;" % (idx, idx),
                "    if (a%d) {" % idx,
                "        if (!rxdk_ansi(&s%d, a%d)) { %s }" % (idx, idx, fail),
                "        p%d = &s%d;" % (idx, idx),
                "    }",
            ]
            if not optional:
                loc.insert(1, "    if (!a%d) { %s }" % (idx, fail))
            expr = "p%d" % idx
            if kind == "oa":
                loc += [
                    "    OBJECT_ATTRIBUTES oa%d;" % idx,
                    "    if (p%d) rxdk_oa(&oa%d, p%d);" % (idx, idx, idx),
                ]
                expr = "p%d ? &oa%d : 0" % (idx, idx)
            return cparams, loc, expr
        if kind == "uni_in":
            cparams = ["const unsigned short *a%d" % idx]
            loc = [
                "    UNICODE_STRING s%d; UNICODE_STRING *p%d = 0;" % (idx, idx),
                "    if (a%d) {" % idx,
                "        if (!rxdk_uni(&s%d, a%d)) { %s }" % (idx, idx, fail),
                "        p%d = &s%d;" % (idx, idx),
                "    }",
            ]
            if not optional:
                loc.insert(1, "    if (!a%d) { %s }" % (idx, fail))
            return cparams, loc, "p%d" % idx
        if kind in ("str_out", "str_inout"):
            cparams = []
            loc = [
                "    char buf%d[1024]; ANSI_STRING s%d;" % (idx, idx),
                "    s%d.Buffer = buf%d; s%d.Length = 0; s%d.MaximumLength = sizeof(buf%d);" % (idx, idx, idx, idx, idx),
            ]
            if kind == "str_inout":
                cparams.append("const char *in%d" % idx)
                loc += [
                    "    if (in%d && !rxdk_ansi(&s%d, in%d)) { %s }" % (idx, idx, idx, fail),
                    "    if (in%d) { unsigned int c; for (c = 0; c < s%d.Length && c + 1 < sizeof(buf%d); c++) buf%d[c] = in%d[c]; s%d.Buffer = buf%d; }" % (
                        idx, idx, idx, idx, idx, idx, idx),
                ]
            cparams += ["char *out%d" % idx, "unsigned int cap%d" % idx, "unsigned int *n%d" % idx]
            return cparams, loc, "&s%d" % idx
        if kind in ("uni_out", "uni_inout"):
            cparams = []
            loc = [
                "    unsigned short ubuf%d[1024]; UNICODE_STRING s%d;" % (idx, idx),
                "    s%d.Buffer = ubuf%d; s%d.Length = 0; s%d.MaximumLength = sizeof(ubuf%d);" % (idx, idx, idx, idx, idx),
            ]
            if kind == "uni_inout":
                cparams.append("const unsigned short *in%d" % idx)
                loc.append("    if (in%d && !rxdk_uni(&s%d, in%d)) { %s }" % (idx, idx, idx, fail))
            cparams += ["char *out%d" % idx, "unsigned int cap%d" % idx, "unsigned int *n%d" % idx]
            return cparams, loc, "&s%d" % idx
        if kind in ("cstr_out", "cstr_inout"):
            cparams = []
            loc = ["    char buf%d[1024];" % idx]
            if kind == "cstr_inout":
                cparams.append("const char *in%d" % idx)
            cparams += ["char *out%d" % idx, "unsigned int cap%d" % idx, "unsigned int *n%d" % idx]
            return cparams, loc, "buf%d" % idx
        if kind == "cstr_in":
            return ["const char *a%d" % idx], [], "a%d" % idx
        if kind == "i64":
            return ["long long a%d" % idx], ["    LARGE_INTEGER v%d; v%d.QuadPart = a%d;" % (idx, idx, idx)], "v%d" % idx
        if kind == "u64":
            return ["unsigned long long a%d" % idx], ["    ULARGE_INTEGER v%d; v%d.QuadPart = a%d;" % (idx, idx, idx)], "v%d" % idx
        if kind == "bool":
            return ["int a%d" % idx], [], "(BOOLEAN)(a%d ? 1 : 0)" % idx
        if kind == "u8":
            return ["unsigned int a%d" % idx], [], "(UCHAR)a%d" % idx
        if kind == "u16":
            return ["unsigned int a%d" % idx], [], "(USHORT)a%d" % idx
        if kind in ("u8_out", "u8_ref"):
            return ["unsigned int *a%d" % idx], ["    UCHAR tmp%d = 0;" % idx], "&tmp%d" % idx
        if kind in ("bool_out", "bool_ref"):
            return ["int *a%d" % idx], ["    BOOLEAN tmp%d = 0;" % idx], "&tmp%d" % idx
        ct = c_type(kind)
        return ["%s a%d" % (ct, idx)], [], "a%d" % idx

    def after_call(idx, kind):
        if kind in ("str_out", "str_inout"):
            return ["    rxdk_copy_ansi(out%d, cap%d, n%d, &s%d);" % (idx, idx, idx, idx)]
        if kind in ("uni_out", "uni_inout"):
            return [
                "    if (n%d) *n%d = s%d.Length;" % (idx, idx, idx),
                "    if (out%d && cap%d) { unsigned int c; unsigned int len = s%d.Length; if (len > cap%d) len = cap%d; for (c = 0; c < len; c++) out%d[c] = ((char *)s%d.Buffer)[c]; if (len < cap%d) out%d[len] = 0; }" % (
                    idx, idx, idx, idx, idx, idx, idx, idx, idx),
            ]
        if kind in ("cstr_out", "cstr_inout"):
            return [
                "    if (n%d) { unsigned int c = 0; while (buf%d[c] && c < sizeof(buf%d)) c++; *n%d = c; }" % (idx, idx, idx, idx),
                "    if (out%d && cap%d) { unsigned int c; for (c = 0; c + 1 < cap%d && buf%d[c]; c++) out%d[c] = buf%d[c]; out%d[c] = 0; if (n%d) *n%d = c; }" % (
                    idx, idx, idx, idx, idx, idx, idx, idx, idx),
            ]
        if kind in ("u8_out", "u8_ref"):
            return ["    if (a%d) *a%d = tmp%d;" % (idx, idx, idx)]
        if kind in ("bool_out", "bool_ref"):
            return ["    if (a%d) *a%d = tmp%d ? 1 : 0;" % (idx, idx, idx)]
        return []

    symbols = []
    for fn in funcs:
        cparams = []
        locs = []
        call_args = []
        posts = []
        for i, p in enumerate(fn["params"]):
            cp, loc, expr = prep_string(i, p["kind"], p.get("optional"), fn["ret"])
            cparams += cp
            locs += loc
            call_args.append(expr)
            posts += after_call(i, p["kind"])
        ret_c = {
            "void": "void", "bool": "int", "i32": "int", "u32": "unsigned int",
            "u8": "unsigned int", "u16": "unsigned int", "i64": "long long",
            "u64": "unsigned long long", "ptr": "void *",
        }[fn["ret"]]
        a("static %s rxdk_k_%s(%s)" % (ret_c, fn["name"], ", ".join(cparams) or "void"))
        a("{")
        for loc in locs:
            a(loc)
        call = "%s(%s)" % (fn["name"], ", ".join(call_args))
        if fn["name"] == "DbgPrint":
            call = 'DbgPrint("%s", a0 ? a0 : "")'
        if fn["ret"] == "void":
            a("    %s;" % call)
        elif fn["ret"] == "bool":
            a("    return %s ? 1 : 0;" % call)
        elif fn["ret"] == "i64":
            a("    return (long long)(%s).QuadPart;" % call)
        elif fn["ret"] == "u64":
            a("    return (unsigned long long)(%s).QuadPart;" % call)
        elif fn["ret"] == "ptr":
            a("    return (void *)(%s);" % call)
        elif fn["ret"] == "u32" or fn["ret"] in ("u8", "u16"):
            a("    return (unsigned int)(%s);" % call)
        else:
            a("    return (int)(%s);" % call)
        for post in posts:
            a(post)
        if fn["ret"] == "void":
            pass
        elif posts:
            # posts run after a returned call; restructure is handled below
            pass
        a("}")
        a("")
        symbols.append("rxdk_k_%s" % fn["name"])

    # The simple return-then-post pattern is wrong. Rebuild function bodies
    # properly by a second pass stored during generation. See emit rewrite below.
    return lines, symbols

def c_cast(param, expr):
    if param["kind"] in ("bool", "u8", "u16", "i64", "u64"):
        return expr
    ty = param["type"].replace("CONST", " ").replace("const", " ")
    ty = " ".join(ty.split())
    if not ty or ty == "void":
        return expr
    return "(%s)(%s)" % (ty, expr)

def is_quad_return(fn):
    raw = " ".join(fn.get("ret_raw", "").split())
    return raw.endswith("LARGE_INTEGER")

def emit_c_funcs(funcs):
    """Emit wrappers whose out-params are written after the kernel call."""
    out = []
    symbols = []
    for fn in funcs:
        if fn.get("sprintf"):
            name = fn["name"]
            out.append("static void rxdk_k_%s(const char *text, char *dst, unsigned int cap, unsigned int *n)" % name)
            out.append("{")
            out.append("    char buf[1024];")
            out.append("    unsigned int i, len;")
            if name in ("RtlSnprintf", "RtlVsnprintf"):
                out.append('    %s(buf, sizeof(buf), "%%s", text ? text : "");' % name)
            else:
                out.append('    %s(buf, "%%s", text ? text : "");' % name)
            out.append("    len = 0;")
            out.append("    while (buf[len] && len + 1 < sizeof(buf)) len++;")
            out.append("    if (n) *n = len;")
            out.append("    if (dst && cap) {")
            out.append("        if (len >= cap) len = cap - 1;")
            out.append("        for (i = 0; i < len; i++) dst[i] = buf[i];")
            out.append("        dst[len] = 0;")
            out.append("    }")
            out.append("}")
            out.append("")
            symbols.append(("rxdk_k_%s" % name, "rxdk_k_%s" % name))
            continue
        cparams = []
        locs = []
        call_args = []
        posts = []
        ret = fn["ret"]
        fail = fail_expr(ret)
        for i, p in enumerate(fn["params"]):
            kind = p["kind"]
            opt = p.get("optional")
            if kind in ("str_in", "oa"):
                cparams.append("const char *a%d" % i)
                locs.append("    ANSI_STRING s%d; ANSI_STRING *p%d = 0;" % (i, i))
                if not opt:
                    locs.append("    if (!a%d) { %s }" % (i, fail))
                locs.append("    if (a%d) {" % i)
                locs.append("        if (!rxdk_ansi(&s%d, a%d)) { %s }" % (i, i, fail))
                locs.append("        p%d = &s%d;" % (i, i))
                locs.append("    }")
                if kind == "oa":
                    locs.append("    OBJECT_ATTRIBUTES oa%d;" % i)
                    locs.append("    if (p%d) rxdk_oa(&oa%d, p%d);" % (i, i, i))
                    call_args.append(c_cast(p, "p%d ? &oa%d : 0" % (i, i)))
                else:
                    call_args.append(c_cast(p, "p%d" % i))
            elif kind == "cstr_in":
                cparams.append("const char *a%d" % i)
                if not opt:
                    locs.append("    if (!a%d) { %s }" % (i, fail))
                call_args.append(c_cast(p, "a%d" % i))
            elif kind == "uni_in":
                cparams.append("const unsigned short *a%d" % i)
                locs.append("    UNICODE_STRING s%d; UNICODE_STRING *p%d = 0;" % (i, i))
                if not opt:
                    locs.append("    if (!a%d) { %s }" % (i, fail))
                locs.append("    if (a%d) {" % i)
                locs.append("        if (!rxdk_uni(&s%d, a%d)) { %s }" % (i, i, fail))
                locs.append("        p%d = &s%d;" % (i, i))
                locs.append("    }")
                call_args.append(c_cast(p, "p%d" % i))
            elif kind in ("str_out", "str_inout"):
                if kind == "str_inout":
                    cparams.append("const char *in%d" % i)
                cparams += ["char *out%d" % i, "unsigned int cap%d" % i, "unsigned int *n%d" % i]
                locs.append("    char buf%d[1024]; ANSI_STRING s%d;" % (i, i))
                locs.append("    s%d.Buffer = buf%d; s%d.Length = 0; s%d.MaximumLength = (USHORT)sizeof(buf%d);" % (i, i, i, i, i))
                if kind == "str_inout":
                    locs.append("    if (in%d) {" % i)
                    locs.append("        if (!rxdk_ansi(&s%d, in%d)) { %s }" % (i, i, fail))
                    locs.append("        { unsigned int c; for (c = 0; c < s%d.Length; c++) buf%d[c] = in%d[c]; s%d.Buffer = buf%d; }" % (i, i, i, i, i))
                    locs.append("    }")
                call_args.append(c_cast(p, "&s%d" % i))
                posts.append("    rxdk_copy_ansi(out%d, cap%d, n%d, &s%d);" % (i, i, i, i))
            elif kind in ("uni_out", "uni_inout"):
                if kind == "uni_inout":
                    cparams.append("const unsigned short *in%d" % i)
                cparams += ["char *out%d" % i, "unsigned int cap%d" % i, "unsigned int *n%d" % i]
                locs.append("    unsigned short ubuf%d[512]; UNICODE_STRING s%d;" % (i, i))
                locs.append("    s%d.Buffer = ubuf%d; s%d.Length = 0; s%d.MaximumLength = (USHORT)sizeof(ubuf%d);" % (i, i, i, i, i))
                if kind == "uni_inout":
                    locs.append("    if (in%d && !rxdk_uni(&s%d, in%d)) { %s }" % (i, i, fail))
                    locs.append("    if (in%d) { unsigned int c; for (c = 0; in%d[c] && c < 511; c++) ubuf%d[c] = in%d[c]; s%d.Length = (USHORT)(c * 2); s%d.Buffer = ubuf%d; }" % (
                        i, i, i, i, i, i, i))
                call_args.append(c_cast(p, "&s%d" % i))
                posts.append("    { unsigned int len = s%d.Length; if (n%d) *n%d = len; if (out%d && cap%d) { unsigned int c; if (len > cap%d) len = cap%d; for (c = 0; c < len; c++) out%d[c] = ((char *)s%d.Buffer)[c]; } }" % (
                    i, i, i, i, i, i, i, i, i))
            elif kind in ("cstr_out", "cstr_inout"):
                if kind == "cstr_inout":
                    cparams.append("const char *in%d" % i)
                cparams += ["char *out%d" % i, "unsigned int cap%d" % i, "unsigned int *n%d" % i]
                locs.append("    char buf%d[1024]; unsigned int blen%d;" % (i, i))
                locs.append("    buf%d[0] = 0;" % i)
                if kind == "cstr_inout":
                    locs.append("    if (in%d) { unsigned int c; for (c = 0; in%d[c] && c + 1 < sizeof(buf%d); c++) buf%d[c] = in%d[c]; buf%d[c] = 0; }" % (
                        i, i, i, i, i, i))
                call_args.append(c_cast(p, "buf%d" % i))
                # Length argument was folded; pass the buffer size to the kernel if this is DbgPrompt-style.
                # The kernel writes into buf. Copy out after.
                posts.append("    blen%d = 0; while (buf%d[blen%d] && blen%d + 1 < sizeof(buf%d)) blen%d++;" % (i, i, i, i, i, i))
                posts.append("    if (n%d) *n%d = blen%d;" % (i, i, i))
                posts.append("    if (out%d && cap%d) { unsigned int c; for (c = 0; c < blen%d && c + 1 < cap%d; c++) out%d[c] = buf%d[c]; out%d[c] = 0; }" % (
                    i, i, i, i, i, i, i))
            elif kind == "i64":
                cparams.append("long long a%d" % i)
                locs.append("    LARGE_INTEGER v%d; v%d.QuadPart = a%d;" % (i, i, i))
                call_args.append(c_cast(p, "v%d" % i))
            elif kind == "u64":
                cparams.append("unsigned long long a%d" % i)
                locs.append("    ULARGE_INTEGER v%d; v%d.QuadPart = a%d;" % (i, i, i))
                call_args.append(c_cast(p, "v%d" % i))
            elif kind == "bool":
                cparams.append("int a%d" % i)
                call_args.append("(BOOLEAN)(a%d ? 1 : 0)" % i)
            elif kind == "u8":
                cparams.append("unsigned int a%d" % i)
                call_args.append("(UCHAR)a%d" % i)
            elif kind == "u16":
                cparams.append("unsigned int a%d" % i)
                call_args.append("(USHORT)a%d" % i)
            elif kind in ("u8_out", "u8_ref"):
                cparams.append("unsigned int *a%d" % i)
                locs.append("    UCHAR tmp%d = 0;" % i)
                call_args.append(c_cast(p, "&tmp%d" % i))
                posts.append("    if (a%d) *a%d = tmp%d;" % (i, i, i))
            elif kind in ("bool_out", "bool_ref"):
                cparams.append("int *a%d" % i)
                locs.append("    BOOLEAN tmp%d = 0;" % i)
                call_args.append(c_cast(p, "&tmp%d" % i))
                posts.append("    if (a%d) *a%d = tmp%d ? 1 : 0;" % (i, i, i))
            elif kind == "iosb" or kind == "iosb_ref":
                cparams.append("IO_STATUS_BLOCK *a%d" % i)
                call_args.append(c_cast(p, "a%d" % i))
            else:
                ct = c_type(kind)
                cparams.append("%s a%d" % (ct, i))
                call_args.append(c_cast(p, "a%d" % i))
            if kind in ("cstr_out", "cstr_inout") and p.get("folded_len"):
                call_args.append("(ULONG)sizeof(buf%d)" % i)
        # DbgPrompt's folded length is already appended when folded_len is set.
        ret_c = {
            "void": "void", "bool": "int", "i32": "int", "u32": "unsigned int",
            "u8": "unsigned int", "u16": "unsigned int", "i64": "long long",
            "u64": "unsigned long long", "ptr": "void *",
        }[ret]
        out.append("static %s rxdk_k_%s(%s)" % (ret_c, fn["name"], ", ".join(cparams) or "void"))
        out.append("{")
        out.extend(locs)
        if fn["name"] == "DbgPrint":
            call = 'DbgPrint("%s", a0 ? a0 : "")'
        else:
            call = "%s(%s)" % (fn["name"], ", ".join(call_args))
        if ret == "void":
            out.append("    %s;" % call)
            out.extend(posts)
        elif not posts:
            if ret == "bool":
                out.append("    return %s ? 1 : 0;" % call)
            elif ret == "i64" and is_quad_return(fn):
                out.append("    { LARGE_INTEGER r = %s; return r.QuadPart; }" % call)
            elif ret == "u64" and is_quad_return(fn):
                out.append("    { ULARGE_INTEGER r = %s; return r.QuadPart; }" % call)
            elif ret == "i64":
                out.append("    return (long long)(%s);" % call)
            elif ret == "u64":
                out.append("    return (unsigned long long)(%s);" % call)
            elif ret == "ptr":
                out.append("    return (void *)(%s);" % call)
            elif ret in ("u32", "u8", "u16"):
                out.append("    return (unsigned int)(%s);" % call)
            else:
                out.append("    return (int)(%s);" % call)
        else:
            if ret == "bool":
                out.append("    int result = %s ? 1 : 0;" % call)
            elif ret == "i64" and is_quad_return(fn):
                out.append("    long long result; { LARGE_INTEGER r = %s; result = r.QuadPart; }" % call)
            elif ret == "u64" and is_quad_return(fn):
                out.append("    unsigned long long result; { ULARGE_INTEGER r = %s; result = r.QuadPart; }" % call)
            elif ret == "i64":
                out.append("    long long result = (long long)(%s);" % call)
            elif ret == "u64":
                out.append("    unsigned long long result = (unsigned long long)(%s);" % call)
            elif ret == "ptr":
                out.append("    void *result = (void *)(%s);" % call)
            elif ret in ("u32", "u8", "u16"):
                out.append("    unsigned int result = (unsigned int)(%s);" % call)
            else:
                out.append("    int result = (int)(%s);" % call)
            out.extend(posts)
            out.append("    return result;")
        out.append("}")
        out.append("")
        symbols.append(("rxdk_k_%s" % fn["name"], "rxdk_k_%s" % fn["name"]))
    return out, symbols

def native_params(fn):
    if fn.get("sprintf"):
        return ["IntPtr text, IntPtr buf, int cap, out int n"]
    params = []
    for i, p in enumerate(fn["params"]):
        kind = p["kind"]
        if kind in ("str_out", "uni_out", "cstr_out"):
            params.append("IntPtr buf%d, int cap%d, out int n%d" % (i, i, i))
        elif kind in ("str_inout", "uni_inout", "cstr_inout"):
            params.append("IntPtr in%d, IntPtr buf%d, int cap%d, out int n%d" % (i, i, i, i))
        elif kind in ("str_in", "cstr_in", "oa", "uni_in"):
            params.append("IntPtr a%d" % i)
        elif kind == "bool":
            params.append("int a%d" % i)
        elif kind in ("u8", "u16", "u32"):
            params.append("uint a%d" % i)
        elif kind == "i32":
            params.append("int a%d" % i)
        elif kind == "i64":
            params.append("long a%d" % i)
        elif kind == "u64":
            params.append("ulong a%d" % i)
        elif kind in ("ptr",):
            params.append("IntPtr a%d" % i)
        elif kind in ("ptr_out", "ptr_ref"):
            params.append("out IntPtr a%d" % i if kind == "ptr_out" else "ref IntPtr a%d" % i)
        elif kind in ("bool_out", "bool_ref"):
            params.append(("out int a%d" if kind.endswith("_out") else "ref int a%d") % i)
        elif kind in ("i32_out", "i32_ref"):
            params.append(("out int a%d" if "out" in kind else "ref int a%d") % i)
        elif kind in ("u32_out", "u32_ref", "u8_out", "u8_ref", "u16_out", "u16_ref"):
            params.append(("out uint a%d" if kind.endswith("_out") else "ref uint a%d") % i)
        elif kind in ("i64_out", "i64_ref"):
            params.append(("out long a%d" if kind.endswith("_out") else "ref long a%d") % i)
        elif kind in ("u64_out", "u64_ref"):
            params.append(("out ulong a%d" if kind.endswith("_out") else "ref ulong a%d") % i)
        elif kind in ("iosb", "iosb_ref"):
            params.append(("out IoStatus a%d" if kind == "iosb" else "ref IoStatus a%d") % i)
        else:
            params.append("IntPtr a%d" % i)
    return params

def public_body(fn):
    if fn.get("sprintf"):
        return "\n".join([
            "        public static unsafe string %s(string text)" % fn["name"],
            "        {",
            "            if (text == null) throw new ArgumentNullException(\"text\");",
            "            byte[] inn = ToAnsi(text);",
            "            byte[] buf = new byte[1024];",
            "            int n;",
            "            fixed (byte* pi = inn, po = buf)",
            "                Native.%s((IntPtr)pi, (IntPtr)po, buf.Length, out n);" % fn["name"],
            "            return FromAnsi(buf, n);",
            "        }",
        ])
    name = fn["name"]
    args = []
    call = []
    pre = []
    post = []
    pins = []
    for i, p in enumerate(fn["params"]):
        cn = camel(p["name"])
        kind = p["kind"]
        if kind in ("str_in", "cstr_in", "oa", "uni_in"):
            args.append(cs_public(kind, p["name"]))
            conv = "ToUtf16" if kind == "uni_in" else "ToAnsi"
            if not p.get("optional"):
                pre.append('            if (%s == null) throw new ArgumentNullException("%s");' % (cn, cn))
                pre.append("            byte[] pin%d = %s(%s);" % (i, conv, cn))
                call.append("(IntPtr)p%d" % i)
            else:
                pre.append("            byte[] pin%d = %s == null ? new byte[1] : %s(%s);" % (i, cn, conv, cn))
                call.append("%s == null ? IntPtr.Zero : (IntPtr)p%d" % (cn, i))
            pins.append("p%d = pin%d" % (i, i))
        elif kind in ("str_out", "uni_out", "cstr_out"):
            args.append(cs_public(kind, p["name"]))
            pre.append("            byte[] pin%d = new byte[1024];" % i)
            pre.append("            int n%d;" % i)
            pins.append("p%d = pin%d" % (i, i))
            call.append("(IntPtr)p%d, pin%d.Length, out n%d" % (i, i, i))
            conv = "FromUtf16" if kind.startswith("uni") else "FromAnsi"
            post.append("            %s = %s(pin%d, n%d);" % (cn, conv, i, i))
        elif kind in ("str_inout", "uni_inout", "cstr_inout"):
            args.append(cs_public(kind, p["name"]))
            conv_in = "ToUtf16" if kind.startswith("uni") else "ToAnsi"
            pre.append("            byte[] inn%d = %s == null ? new byte[1] : %s(%s);" % (i, cn, conv_in, cn))
            pre.append("            byte[] pin%d = new byte[1024];" % i)
            pre.append("            int n%d;" % i)
            pins.append("ip%d = inn%d" % (i, i))
            pins.append("p%d = pin%d" % (i, i))
            call.append("%s == null ? IntPtr.Zero : (IntPtr)ip%d, (IntPtr)p%d, pin%d.Length, out n%d" % (cn, i, i, i, i))
            conv = "FromUtf16" if kind.startswith("uni") else "FromAnsi"
            post.append("            %s = %s(pin%d, n%d);" % (cn, conv, i, i))
        elif kind == "bool":
            args.append(cs_public(kind, p["name"]))
            call.append("%s ? 1 : 0" % cn)
        elif kind in ("u8",):
            args.append(cs_public(kind, p["name"]))
            call.append("(uint)%s" % cn)
        elif kind in ("u16",):
            args.append(cs_public(kind, p["name"]))
            call.append("(uint)%s" % cn)
        elif kind in ("bool_out", "bool_ref"):
            args.append(cs_public(kind, p["name"]))
            pre.append("            int raw%d;" % i)
            if kind.endswith("_ref"):
                pre.append("            raw%d = %s ? 1 : 0;" % (i, cn))
                call.append("ref raw%d" % i)
            else:
                call.append("out raw%d" % i)
            post.append("            %s = raw%d != 0;" % (cn, i))
        elif kind in ("u8_out", "u16_out"):
            args.append(cs_public(kind, p["name"]))
            pre.append("            uint raw%d;" % i)
            call.append("out raw%d" % i)
            cast = "(byte)" if kind.startswith("u8") else "(ushort)"
            post.append("            %s = %sraw%d;" % (cn, cast, i))
        elif kind in ("u8_ref", "u16_ref"):
            args.append(cs_public(kind, p["name"]))
            pre.append("            uint raw%d = %s;" % (i, cn))
            call.append("ref raw%d" % i)
            cast = "(byte)" if kind.startswith("u8") else "(ushort)"
            post.append("            %s = %sraw%d;" % (cn, cast, i))
        else:
            args.append(cs_public(kind, p["name"]))
            prefix = ""
            if kind.endswith("_out"):
                prefix = "out "
            elif kind.endswith("_ref") or kind in ("iosb_ref",):
                prefix = "ref "
            elif kind == "iosb":
                prefix = "out "
            call.append(prefix + cn)
    ret = fn["ret"]
    cs_ret = {"void": "void", "bool": "bool", "i32": "int", "u32": "uint", "u8": "uint",
              "u16": "uint", "i64": "long", "u64": "ulong", "ptr": "IntPtr"}[ret]
    unsafe = "unsafe " if pins else ""
    sig = "        public static %s%s %s(%s)" % (unsafe, cs_ret, name, ", ".join(args))
    body = []
    body.append(sig)
    body.append("        {")
    body.extend(pre)
    invocation = "Native.%s(%s)" % (name, ", ".join(call))
    if pins:
        if ret == "void":
            body.append("            fixed (byte* %s)" % ", ".join(pins))
            body.append("                %s;" % invocation)
            body.extend(post)
        else:
            hold = "bool" if ret == "bool" else cs_ret
            expr = ("%s != 0" % invocation) if ret == "bool" else invocation
            body.append("            %s result;" % hold)
            body.append("            fixed (byte* %s)" % ", ".join(pins))
            body.append("                result = %s;" % expr)
            body.extend(post)
            body.append("            return result;")
    else:
        if ret == "void" and not post:
            body.append("            %s;" % invocation)
        elif ret == "void":
            body.append("            %s;" % invocation)
            body.extend(post)
        elif not post:
            if ret == "bool":
                body.append("            return %s != 0;" % invocation)
            else:
                body.append("            return %s;" % invocation)
        else:
            if ret == "bool":
                body.append("            bool result = %s != 0;" % invocation)
            else:
                body.append("            %s result = %s;" % (cs_ret, invocation))
            body.extend(post)
            body.append("            return result;")
    body.append("        }")
    return "\n".join(body)

def emit_cs(funcs, data):
    lines = []
    a = lines.append
    a("// Generated by scripts/gen-kernel.py. Do not edit.")
    a("// Kernel exports as managed methods. Names match the kernel. Strings, bools,")
    a("// and out scalars are ordinary C# values; the native wrapper holds the")
    a("// ANSI_STRING / OBJECT_ATTRIBUTES / IO_STATUS_BLOCK for the call. Handles and")
    a("// kernel objects stay IntPtr. File, thread, and socket code stays on the")
    a("// APIs that already cover it (sockets are libxnet).")
    a("using System;")
    a("using System.Runtime.InteropServices;")
    a("")
    a("namespace Rxdk")
    a("{")
    a("    [StructLayout(LayoutKind.Sequential)]")
    a("    public struct IoStatus")
    a("    {")
    a("        public int Status;")
    a("        public uint Information;")
    a("    }")
    a("")
    a("    public static class Kernel")
    a("    {")
    a("        public const uint HalHaltRoutine = 0;")
    a("        public const uint HalRebootRoutine = 1;")
    a("        public const uint HalQuickRebootRoutine = 2;")
    a("        public const uint HalKdRebootRoutine = 3;")
    a("        public const uint HalFatalErrorRebootRoutine = 4;")
    a("")
    a("        public const uint TrayClosed = 0x00;")
    a("        public const uint TrayOpen = 0x10;")
    a("        public const uint TrayUnloading = 0x20;")
    a("        public const uint TrayOpening = 0x30;")
    a("        public const uint TrayNoMedia = 0x40;")
    a("        public const uint TrayClosing = 0x50;")
    a("        public const uint TrayMediaDetect = 0x60;")
    a("")
    a("        public static IntPtr NtCurrentThread()")
    a("        {")
    a("            return (IntPtr)(-2);")
    a("        }")
    a("")
    for fn in funcs:
        a(public_body(fn))
        a("")
    for d in data:
        a(data_property(d))
        a("")
    a("        static byte[] ToAnsi(string text)")
    a("        {")
    a("            byte[] buf = new byte[text.Length + 1];")
    a("            for (int i = 0; i < text.Length; i++)")
    a("                buf[i] = (byte)text[i];")
    a("            return buf;")
    a("        }")
    a("")
    a("        static byte[] ToUtf16(string text)")
    a("        {")
    a("            byte[] buf = new byte[(text.Length + 1) * 2];")
    a("            for (int i = 0; i < text.Length; i++) {")
    a("                buf[i * 2] = (byte)text[i];")
    a("                buf[i * 2 + 1] = (byte)((int)text[i] >> 8);")
    a("            }")
    a("            return buf;")
    a("        }")
    a("")
    a("        static string FromAnsi(byte[] buf, int length)")
    a("        {")
    a("            if (length < 0) length = 0;")
    a("            if (length > buf.Length) length = buf.Length;")
    a("            char[] chars = new char[length];")
    a("            int n = 0;")
    a("            for (int i = 0; i < length; i++) {")
    a("                if (buf[i] == 0) break;")
    a("                chars[i] = (char)buf[i];")
    a("                n++;")
    a("            }")
    a("            return new string(chars, 0, n);")
    a("        }")
    a("")
    a("        static string FromUtf16(byte[] buf, int byteLength)")
    a("        {")
    a("            if (byteLength < 0) byteLength = 0;")
    a("            if (byteLength > buf.Length) byteLength = buf.Length;")
    a("            int n = byteLength / 2;")
    a("            char[] chars = new char[n];")
    a("            for (int i = 0; i < n; i++)")
    a("                chars[i] = (char)(buf[i * 2] | (buf[i * 2 + 1] << 8));")
    a("            return new string(chars);")
    a("        }")
    a("")
    a("        static class Native")
    a("        {")
    for fn in funcs:
        nparams = ", ".join(native_params(fn))
        cs_ret = {"void": "void", "bool": "int", "i32": "int", "u32": "uint", "u8": "uint",
                  "u16": "uint", "i64": "long", "u64": "ulong", "ptr": "IntPtr"}[fn["ret"]]
        a("            [DllImport(\"xboxkrnl\", EntryPoint = \"rxdk_k_%s\", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, CharSet = CharSet.Ansi)]" % fn["name"])
        a("            internal static extern %s %s(%s);" % (cs_ret, fn["name"], nparams))
        a("")
    for d in data:
        a(data_native(d))
        a("")
    a("        }")
    a("    }")
    a("}")
    a("")
    return "\n".join(lines)

def data_kind(d):
    t = d["type"]
    if d["array"]:
        return "ptr"
    if t in ("STRING", "ANSI_STRING", "OBJECT_STRING"):
        return "str"
    if t in ("DWORD", "ULONG", "UINT", "BOOLEAN", "BOOL", "BYTE", "UCHAR"):
        return "u32"
    return "ptr"

def data_property(d):
    name = d["name"]
    kind = data_kind(d)
    if kind == "str":
        return "\n".join([
            "        public static unsafe string %s" % name,
            "        {",
            "            get",
            "            {",
            "                byte[] buf = new byte[128];",
            "                int n;",
            "                fixed (byte* p = buf)",
            "                    Native.rxdk_d_%s((IntPtr)p, buf.Length, out n);" % name,
            "                return FromAnsi(buf, n);",
            "            }",
            "        }",
        ])
    if kind == "u32":
        return "\n".join([
            "        public static uint %s" % name,
            "        {",
            "            get { return Native.rxdk_d_%s(); }" % name,
            "        }",
        ])
    return "\n".join([
        "        public static IntPtr %s" % name,
        "        {",
        "            get { return Native.rxdk_d_%s(); }" % name,
        "        }",
    ])

def data_native(d):
    kind = data_kind(d)
    name = d["name"]
    if kind == "str":
        sig = "void rxdk_d_%s(IntPtr buf, int cap, out int n)" % name
    elif kind == "u32":
        sig = "uint rxdk_d_%s()" % name
    else:
        sig = "IntPtr rxdk_d_%s()" % name
    return "\n".join([
        "            [DllImport(\"xboxkrnl\", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]",
        "            internal static extern %s;" % sig,
    ])

def emit_c_data(data):
    lines = []
    symbols = []
    for d in data:
        name = d["name"]
        kind = data_kind(d)
        if kind == "str":
            lines.append("static void rxdk_d_%s(char *buf, unsigned int cap, unsigned int *n)" % name)
            lines.append("{")
            lines.append("    rxdk_copy_ansi(buf, cap, n, &%s);" % name)
            lines.append("}")
        elif kind == "u32":
            lines.append("static unsigned int rxdk_d_%s(void)" % name)
            lines.append("{")
            lines.append("    return (unsigned int)%s;" % name)
            lines.append("}")
        else:
            lines.append("static void *rxdk_d_%s(void)" % name)
            lines.append("{")
            lines.append("    return (void *)&%s;" % name)
            lines.append("}")
        lines.append("")
        symbols.append("rxdk_d_%s" % name)
    return lines, symbols

def main():
    text = load_headers()
    funcs = parse_functions(text)
    data = parse_data(text)
    # Drop duplicate names, keeping the first.
    seen = set()
    uniq = []
    for fn in funcs:
        if fn["name"] in seen:
            continue
        seen.add(fn["name"])
        uniq.append(fn)
    funcs = uniq
    for fn in funcs:
        if "Append" in fn["name"]:
            for p in fn["params"]:
                if p["kind"] == "str_out" and p["dir"] == "inout":
                    p["kind"] = "str_inout"
                if p["kind"] == "uni_out" and p["dir"] == "inout":
                    p["kind"] = "uni_inout"
        for p in fn["params"]:
            if p["kind"] == "oa":
                p["name"] = "Name"
    warns = []
    for fn in funcs:
        for p in fn["params"]:
            if "warn" in p:
                warns.append("%s %s (%s)" % (fn["name"], p["name"], p["warn"]))
    c_funcs, _ = emit_c_funcs(funcs)
    c_data, data_syms = emit_c_data(data)
    symbols = ["rxdk_k_%s" % fn["name"] for fn in funcs] + data_syms
    header = []
    h = header.append
    h("/* Generated by scripts/gen-kernel.py. Do not edit.")
    h(" * Cdecl wrappers around xboxkrnl. Strings and object attributes live for")
    h(" * the call. libkernel is already on the base link.")
    h(" */")
    h("#include <string.h>")
    h("#include <xboxkrnl/xboxkrnl.h>")
    h("")
    h("typedef void *(*RxdkDlLoad)(const char *name, int flags, char **err, void *ud);")
    h("typedef void *(*RxdkDlSymbol)(void *handle, const char *name, char **err, void *ud);")
    h("typedef void *(*RxdkDlClose)(void *handle, void *ud);")
    h("extern void *mono_dl_fallback_register(RxdkDlLoad, RxdkDlSymbol, RxdkDlClose, void *);")
    h("")
    h("static int rxdk_ansi(ANSI_STRING *s, const char *text)")
    h("{")
    h("    unsigned int n = 0;")
    h("    if (!text) return 0;")
    h("    while (text[n]) {")
    h("        if (n >= 1024) return 0;")
    h("        n++;")
    h("    }")
    h("    s->Length = (USHORT)n;")
    h("    s->MaximumLength = (USHORT)(n + 1);")
    h("    s->Buffer = (PCHAR)text;")
    h("    return 1;")
    h("}")
    h("")
    h("static int rxdk_uni(UNICODE_STRING *s, const unsigned short *text)")
    h("{")
    h("    unsigned int n = 0;")
    h("    if (!text) return 0;")
    h("    while (text[n]) {")
    h("        if (n >= 1024) return 0;")
    h("        n++;")
    h("    }")
    h("    s->Length = (USHORT)(n * 2);")
    h("    s->MaximumLength = (USHORT)((n + 1) * 2);")
    h("    s->Buffer = (PWSTR)text;")
    h("    return 1;")
    h("}")
    h("")
    h("static void rxdk_copy_ansi(char *dst, unsigned int cap, unsigned int *n, STRING *s)")
    h("{")
    h("    unsigned int i, len;")
    h("    len = (s && s->Buffer) ? s->Length : 0;")
    h("    if (!dst || cap == 0) { if (n) *n = len; return; }")
    h("    if (len >= cap) len = cap - 1;")
    h("    for (i = 0; i < len; i++) dst[i] = s->Buffer[i];")
    h("    dst[len] = 0;")
    h("    if (n) *n = len;")
    h("}")
    h("")
    h("static void rxdk_oa(OBJECT_ATTRIBUTES *oa, ANSI_STRING *name)")
    h("{")
    h("    oa->RootDirectory = NULL;")
    h("    oa->ObjectName = name;")
    h("    oa->Attributes = OBJ_CASE_INSENSITIVE;")
    h("}")
    h("")
    body = header + c_funcs + c_data
    body.append("static const struct { const char *name; void *addr; } kernel_syms[] = {")
    for sym in symbols:
        body.append('    { "%s", (void *)&%s },' % (sym, sym))
    body.append("};")
    body.append("")
    body.append("static void *kernel_load(const char *name, int flags, char **err, void *ud)")
    body.append("{")
    body.append("    (void)flags; (void)err; (void)ud;")
    body.append('    if (name && (strcmp(name, "xboxkrnl") == 0 || strcmp(name, "xboxkrnl.dll") == 0))')
    body.append("        return (void *)(size_t)0x4B524E4C;")
    body.append("    return 0;")
    body.append("}")
    body.append("")
    body.append("static void *kernel_symbol(void *handle, const char *name, char **err, void *ud)")
    body.append("{")
    body.append("    unsigned int i;")
    body.append("    (void)handle; (void)err; (void)ud;")
    body.append("    if (!name) return 0;")
    body.append("    for (i = 0; i < sizeof(kernel_syms) / sizeof(kernel_syms[0]); i++) {")
    body.append("        if (!strcmp(kernel_syms[i].name, name))")
    body.append("            return kernel_syms[i].addr;")
    body.append("    }")
    body.append("    return 0;")
    body.append("}")
    body.append("")
    body.append("static void *kernel_close(void *handle, void *ud)")
    body.append("{")
    body.append("    (void)handle; (void)ud;")
    body.append("    return 0;")
    body.append("}")
    body.append("")
    body.append("void rxdk_bind_kernel_register(void)")
    body.append("{")
    body.append("    mono_dl_fallback_register(kernel_load, kernel_symbol, kernel_close, 0);")
    body.append("}")
    body.append("")
    with open(C_PATH, "w", newline="\n") as f:
        f.write("\n".join(body))
    with open(CS_PATH, "w", newline="\n") as f:
        f.write(emit_cs(funcs, data))
    print("functions %d  data %d  warnings %d" % (len(funcs), len(data), len(warns)))
    for w in warns:
        print("warn", w)
    # Show the calls the self-test will use.
    want = {"IoCreateSymbolicLink", "IoDeleteSymbolicLink", "NtOpenSymbolicLinkObject",
            "NtQuerySymbolicLinkObject", "NtClose", "AvGetSavedDataAddress", "AvSetDisplayMode",
            "DbgPrint", "IofCallDriver", "HalReturnToFirmware", "HalDiskModelNumber"}
    for fn in funcs:
        if fn["name"] in want:
            print(fn["name"], fn["ret"], [(p["name"], p["kind"]) for p in fn["params"]])

if __name__ == "__main__":
    main()
