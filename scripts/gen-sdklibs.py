# Generates cdecl wrappers and managed methods for the remaining RXDK libraries.
# A function is emitted only when the header declaration and the stdcall
# export (_Name@N) agree on the stack size. Do not edit the generated files.
import os
import re

SDK = os.environ.get("RXDK_SDK", r"C:\ProgramData\RXDK\sdk")
INC = os.path.join(SDK, "include")
LIB = os.path.join(SDK, "lib")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The lib column lists the library plus the SDK libs it needs on the link line:
# dmusic, xact, and xmv all call into libdsound; uix calls into libxonline.
LIBS = [
    ("dsound", ["dsound.h"], "libdsound.lib", "Rxdk.Audio", "Audio", "rxdk_bind_dsound_register", 0x44534E44),
    ("dmusic", ["dmusici.h"], "libdmusic.lib libdsound.lib", "Rxdk.Music", "Music", "rxdk_bind_dmusic_register", 0x444D5553),
    ("xact", ["xact.h"], "libxact.lib libdsound.lib", "Rxdk.Xact", "Xact", "rxdk_bind_xact_register", 0x58414354),
    ("xmv", ["xmv.h"], "libxmv.lib libdsound.lib", "Rxdk.Video", "Video", "rxdk_bind_xmv_register", 0x584D5600),
    ("uix", ["uix.h"], "libuix.lib libxonline.lib libxneto.lib", "Rxdk.Uix", "Uix", "rxdk_bind_uix_register", 0x55495800),
    # libxonline's CXoBase lives in libxneto, the online-capable build of libxnet
    # (same 57 networking exports, so it substitutes for the base libxnet).
    ("xonline", ["xonline.h"], "libxonline.lib libxneto.lib", "Rxdk.Online", "Online", "rxdk_bind_xonline_register", 0x584F4E4C),
    ("xvoice", ["xvoice.h", "xhv.h"], "libxvoice.lib", "Rxdk.Voice", "Voice", "rxdk_bind_xvoice_register", 0x58564F49),
]
# Interface methods the headers reach only through lpVtbl macros, so no export exists to
# match. Each entry is (name, vtable slot, return kind, parameter kinds); the first parameter
# is the object. IDirectSoundStream derives from XMediaObject, so these drive streams too.
VTABLE = {
    "dsound": [
        ("XMediaObject_AddRef", 0, "i32", ["ptr"]),
        ("XMediaObject_Release", 1, "i32", ["ptr"]),
        ("XMediaObject_GetInfo", 2, "i32", ["ptr", "ptr"]),
        ("XMediaObject_GetStatus", 3, "i32", ["ptr", "ptr"]),
        ("XMediaObject_Process", 4, "i32", ["ptr", "ptr", "ptr"]),
        ("XMediaObject_Discontinuity", 5, "i32", ["ptr"]),
        ("XMediaObject_Flush", 6, "i32", ["ptr"]),
        ("XFileMediaObject_Seek", 7, "i32", ["ptr", "i32", "i32", "ptr"]),
        ("XFileMediaObject_GetLength", 8, "i32", ["ptr", "ptr"]),
        ("XFileMediaObject_DoWork", 9, "void", ["ptr"]),
    ],
}
# libxbdm is deliberately absent. It is an import library that binds xbdm.dll purely
# by ordinal (1..88), so any title linking it fails to load with
# STATUS_ORDINAL_NOT_FOUND unless the installed debug monitor exports those exact
# ordinals. The managed debug surface is not worth an unbootable image.

QUAL = {
    "const", "CONST", "IN", "OUT", "OPTIONAL", "volatile", "VOLATILE",
    "extern", "XBOXAPI", "WINAPI", "__stdcall", "unsigned", "signed",
    "struct", "enum", "union", "class", "long", "short",
    "__in", "__out", "__inout", "__in_opt", "__out_opt", "__inout_opt",
    "__in_ecount", "__out_ecount",
}
KEYWORDS = {
    "abstract", "as", "base", "bool", "break", "byte", "case", "catch",
    "char", "checked", "class", "const", "continue", "decimal", "default",
    "delegate", "do", "double", "else", "enum", "event", "explicit",
    "extern", "false", "finally", "fixed", "float", "for", "foreach",
    "goto", "if", "implicit", "in", "int", "interface", "internal", "is",
    "lock", "long", "namespace", "new", "null", "object", "operator",
    "out", "override", "params", "private", "protected", "public",
    "readonly", "ref", "return", "sbyte", "sealed", "short", "sizeof",
    "stackalloc", "static", "string", "struct", "switch", "this", "throw",
    "true", "try", "typeof", "uint", "ulong", "unchecked", "unsafe",
    "ushort", "using", "virtual", "void", "volatile", "while", "add",
    "remove", "get", "set", "value", "yield", "partial", "var",
}
ANSI = {"LPCSTR", "LPSTR", "PCSTR", "PSTR", "LPCTSTR", "LPCCH", "PCHAR", "LPCH"}
WIDE = {"LPCWSTR", "LPWSTR", "PCWSTR", "PWSTR", "LPWCH", "LPCWCH", "PWCHAR"}
WIDE8 = {
    "LONGLONG", "ULONGLONG", "LARGE_INTEGER", "ULARGE_INTEGER", "__int64",
    "DWORDLONG", "QWORD", "LONG64", "ULONG64", "UINT64", "INT64", "FILETIME",
    "DWORD64", "REFERENCE_TIME", "XOFFERING_ID", "XNKID", "XENTITY_ID",
}
# Xbox XUID is a packed 8-byte id plus a DWORD of flags.
BYVAL = {"XUID": ("xuid", 12)}
WIDE16 = {"GUID", "CLSID", "UUID", "IID"}
PTR_EXACT = {
    "PVOID", "PCVOID", "LPVOID", "LPCVOID", "HANDLE", "HWND", "HDC",
    "HINSTANCE", "HMODULE", "HWAVEOUT", "HWAVEIN",
}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*?$", "", text, flags=re.M)
    return text


def exports(libname):
    # Only the first entry owns exports; the rest are link-line dependencies.
    found = {}
    data = open(os.path.join(LIB, libname.split()[0]), "rb").read()
    for m in re.finditer(rb"_([A-Za-z_][A-Za-z0-9_]*)@(\d+)", data):
        found.setdefault(m.group(1).decode(), set()).add(int(m.group(2)))
    return found


def read_headers(names):
    parts = []
    for name in names:
        raw = open(os.path.join(INC, name), "r", errors="replace").read()
        raw = strip_comments(raw)
        lines = []
        for line in raw.splitlines():
            if line.lstrip().startswith("#"):
                continue
            lines.append(line)
        parts.append("\n".join(lines))
    return "\n".join(parts)


def params_of(text, open_at):
    depth = 0
    i = open_at
    while i < len(text):
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return text[open_at + 1:i]
        i += 1
    return None


def cs_name(name, used):
    base = name
    if base in KEYWORDS:
        base = base + "_"
    n = base
    i = 2
    while n in used:
        n = base + str(i)
        i += 1
    used.add(n)
    return n


def classify_param(raw):
    text = raw.strip()
    text = re.sub(r"\s*=\s*.*$", "", text).strip()
    if not text or text == "void" or text == "VOID":
        return None
    if "..." in text:
        return "varargs"
    # Function pointer: take the identifier after the star.
    fname = None
    fm = re.search(r"\(\s*\*\s*(\w+)\s*\)", text)
    if fm:
        fname = fm.group(1)
        kind, size = "ptr", 4
        return {"name": fname, "kind": kind, "size": size, "string": None}
    tokens = re.findall(r"[A-Za-z_][\w]*|\*", text)
    if not tokens:
        return None
    name = None
    if tokens[-1] != "*" and tokens[-1] not in QUAL and tokens[-1] not in {"void", "VOID", "int", "char", "float", "double", "BOOL", "HRESULT"}:
        # Last ident is the parameter name when a type remains.
        if len(tokens) > 1:
            name = tokens[-1]
            tokens = tokens[:-1]
    stars = tokens.count("*") + text.count("&")
    idents = [t for t in tokens if t != "*" and t not in QUAL]
    if stars or any(t in PTR_EXACT or t.startswith("LP") for t in idents) or any(t.startswith("P") and len(t) > 2 and t[1].isupper() for t in idents):
        kind, size = "ptr", 4
    elif any(t in BYVAL for t in idents):
        kind, size = BYVAL[[t for t in idents if t in BYVAL][0]]
    elif any(t in WIDE8 for t in idents):
        kind, size = "i64", 8
    elif any(t in WIDE16 for t in idents):
        kind, size = "i128", 16
    elif "double" in idents or "DOUBLE" in idents:
        kind, size = "f64", 8
    elif "float" in idents or "FLOAT" in idents:
        kind, size = "f32", 4
    else:
        kind, size = "i32", 4
    string = None
    if kind == "ptr":
        if any(t in WIDE for t in idents) or "wchar_t" in idents or "WCHAR" in idents:
            string = "wide"
        elif any(t in ANSI for t in idents) or ("char" in idents and "unsigned" not in text):
            string = "ansi"
    if kind == "i32" and ("BOOL" in idents or "bool" in idents):
        kind = "bool"
    return {"name": name, "kind": kind, "size": size, "string": string}


def classify_return(ret):
    ret = ret.strip()
    idents = re.findall(r"[A-Za-z_][\w]*", ret)
    if not idents or idents[-1] in {"void", "VOID"}:
        return "void"
    if "*" in ret or any(t in PTR_EXACT or t.startswith("LP") for t in idents):
        return "ptr"
    if "float" in idents or "FLOAT" in idents:
        return "f32"
    if "double" in idents or "DOUBLE" in idents:
        return "f64"
    if any(t in WIDE8 for t in idents):
        return "i64"
    if "BOOL" in idents or idents[-1] == "bool":
        return "bool"
    return "i32"


def parse_functions(text):
    found = []
    patterns = [
        ("stdapi_", re.compile(r"\bSTDAPI_\(\s*([^)]+?)\s*\)\s*([A-Za-z_]\w*)\s*\(")),
        ("stdapi", re.compile(r"\bSTDAPI\s+([A-Za-z_]\w*)\s*\(")),
        ("dmhr", re.compile(r"\bDMHRAPI\s+([A-Za-z_]\w*)\s*\(")),
        ("dmbool", re.compile(r"\bDMAPI\s+BOOL\s+__stdcall\s+([A-Za-z_]\w*)\s*\(")),
        ("winapi", re.compile(r"\b(?:WINAPI|__stdcall)\s+([A-Za-z_]\w*)\s*\(")),
    ]
    occupied = []

    def overlap(a, b):
        for s, e in occupied:
            if a < e and b > s:
                return True
        return False

    for kind, cre in patterns:
        for m in cre.finditer(text):
            if overlap(m.start(), m.end()):
                continue
            if kind == "stdapi_":
                ret, name = m.group(1), m.group(2)
                open_at = m.end() - 1
            elif kind == "stdapi":
                ret, name = "HRESULT", m.group(1)
                open_at = m.end() - 1
            elif kind == "dmhr":
                ret, name = "HRESULT", m.group(1)
                open_at = m.end() - 1
            elif kind == "dmbool":
                ret, name = "BOOL", m.group(1)
                open_at = m.end() - 1
            else:
                name = m.group(1)
                open_at = m.end() - 1
                bound = max(text.rfind(";", 0, m.start()), text.rfind("{", 0, m.start()), text.rfind("}", 0, m.start()))
                ret = text[bound + 1:m.start()]
                ret = re.sub(r"\b(extern|XBOXAPI|WINAPI|__stdcall)\b", "", ret).strip()
            body = params_of(text, open_at)
            if body is None:
                continue
            parts = []
            depth = 0
            cur = []
            for ch in body:
                if ch in "([{":
                    depth += 1
                    cur.append(ch)
                elif ch in ")]}":
                    depth -= 1
                    cur.append(ch)
                elif ch == "," and depth == 0:
                    parts.append("".join(cur))
                    cur = []
                else:
                    cur.append(ch)
            if cur and "".join(cur).strip():
                parts.append("".join(cur))
            params = []
            bad = False
            for part in parts:
                c = classify_param(part)
                if c == "varargs":
                    bad = True
                    break
                if c:
                    params.append(c)
            if bad:
                continue
            found.append({"name": name, "ret": classify_return(ret), "params": params, "at": m.start()})
            occupied.append((m.start(), open_at + len(body) + 2))
    found.sort(key=lambda f: f["at"])
    return found


def c_param(kind):
    return {"ptr": "void *", "i32": "int", "bool": "int", "f32": "float", "f64": "double", "i64": "long long", "i128": "void *", "xuid": "RxdkXuid"}[kind]


def cs_native(kind, string):
    if string == "ansi" or string == "wide":
        return "IntPtr"
    return {"ptr": "IntPtr", "i32": "int", "bool": "int", "f32": "float", "f64": "double", "i64": "long", "i128": "IntPtr", "xuid": "Xuid"}[kind]


def cs_public(kind, string):
    if string:
        return "string"
    if kind == "bool":
        return "bool"
    return cs_native(kind, None)


def emit_cs(library, cls, kept):
    prefix = "rxdk_%s_" % library
    cs = []
    cs.append("// Generated by scripts/gen-sdklibs.py. Do not edit.")
    cs.append("using System;")
    cs.append("using System.Runtime.InteropServices;")
    cs.append("using System.Text;")
    cs.append("")
    cs.append("namespace Rxdk")
    cs.append("{")
    if any(p["kind"] == "xuid" for fn in kept for p in fn["params"]):
        cs.append("    [StructLayout(LayoutKind.Sequential, Pack = 4)]")
        cs.append("    public struct Xuid")
        cs.append("    {")
        cs.append("        public long Id;")
        cs.append("        public int Flags;")
        cs.append("    }")
        cs.append("")
    cs.append("    public static class %s" % cls)
    cs.append("    {")
    native = []
    for fn in kept:
        needs_wrap = fn["ret"] in {"bool", "ptr"} or any(p["string"] or p["kind"] == "bool" for p in fn["params"])
        ret_cs = {"void": "void", "f32": "float", "f64": "double", "i64": "long", "i32": "int", "bool": "bool", "ptr": "IntPtr"}[fn["ret"]]
        nat_ret = {"void": "void", "f32": "float", "f64": "double", "i64": "long", "i32": "int", "bool": "int", "ptr": "IntPtr"}[fn["ret"]]
        if not needs_wrap:
            args = ", ".join("%s %s" % (cs_public(p["kind"], None), p["cs"]) for p in fn["params"])
            cs.append('        [DllImport("%s", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "%s%s")]' % (library, prefix, fn["name"]))
            cs.append("        public static extern %s %s(%s);" % (ret_cs, fn["name"], args))
            cs.append("")
            continue
        pub_args = ", ".join("%s %s" % (cs_public(p["kind"], p["string"]), p["cs"]) for p in fn["params"])
        unsafe = " unsafe" if any(p["string"] for p in fn["params"]) else ""
        cs.append("        public static%s %s %s(%s)" % (unsafe, ret_cs, fn["name"], pub_args))
        cs.append("        {")
        call_args = []
        for p in fn["params"]:
            if p["string"] == "ansi":
                cs.append("            byte[] %sBytes = %s == null ? null : ToAnsi(%s);" % (p["cs"], p["cs"], p["cs"]))
                call_args.append("(%sBytes == null) ? IntPtr.Zero : (IntPtr)%sPtr" % (p["cs"], p["cs"]))
            elif p["string"] == "wide":
                cs.append("            byte[] %sBytes = %s == null ? null : ToWide(%s);" % (p["cs"], p["cs"], p["cs"]))
                call_args.append("(%sBytes == null) ? IntPtr.Zero : (IntPtr)%sPtr" % (p["cs"], p["cs"]))
            elif p["kind"] == "bool":
                call_args.append("%s ? 1 : 0" % p["cs"])
            else:
                call_args.append(p["cs"])
        strings = [p for p in fn["params"] if p["string"]]
        indent = "            "
        for p in strings:
            cs.append("%sfixed (byte* %sPtr = %sBytes)" % (indent, p["cs"], p["cs"]))
            indent += "    "
        call = "Native.%s(%s)" % (fn["name"], ", ".join(call_args))
        if fn["ret"] == "void":
            cs.append("%s%s;" % (indent, call))
        elif fn["ret"] == "bool":
            cs.append("%sreturn %s != 0;" % (indent, call))
        else:
            cs.append("%sreturn %s;" % (indent, call))
        cs.append("        }")
        cs.append("")
        nat_args = ", ".join("%s %s" % (cs_native(p["kind"], p["string"]), p["cs"]) for p in fn["params"])
        native.append('            [DllImport("%s", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "%s%s")]' % (library, prefix, fn["name"]))
        native.append("            public static extern %s %s(%s);" % (nat_ret, fn["name"], nat_args))
        native.append("")
    if any(p["string"] == "ansi" for fn in kept for p in fn["params"]):
        cs.append("        static byte[] ToAnsi(string text)")
        cs.append("        {")
        cs.append("            return Encoding.ASCII.GetBytes(text + \"\\0\");")
        cs.append("        }")
        cs.append("")
    if any(p["string"] == "wide" for fn in kept for p in fn["params"]):
        cs.append("        static byte[] ToWide(string text)")
        cs.append("        {")
        cs.append("            return Encoding.Unicode.GetBytes(text + \"\\0\");")
        cs.append("        }")
        cs.append("")
    if native:
        cs.append("        static class Native")
        cs.append("        {")
        cs.extend(native)
        cs.append("        }")
    cs.append("    }")
    cs.append("}")
    cs.append("")
    return "\n".join(cs)


def main():
    os.makedirs(os.path.join(ROOT, "pal", "src", "bind"), exist_ok=True)
    for lib in LIBS:
        library, headers, libfile, assembly, cls, register, cookie = lib
        ex = exports(libfile)
        funcs = parse_functions(read_headers(headers))
        kept = []
        seen = set()
        skipped = 0
        missing = 0
        for fn in funcs:
            if fn["name"] in seen:
                continue
            sizes = ex.get(fn["name"])
            if not sizes:
                missing += 1
                continue
            stack = sum(p["size"] for p in fn["params"])
            if stack not in sizes or any(p["kind"] == "i128" for p in fn["params"]):
                skipped += 1
                continue
            seen.add(fn["name"])
            used = set()
            for i, p in enumerate(fn["params"]):
                p["cs"] = cs_name(p["name"] or ("a%d" % i), used)
            kept.append(fn)
        for name, slot, ret, kinds in VTABLE.get(library, []):
            if name in seen:
                continue
            seen.add(name)
            params = [{"name": None, "kind": k, "size": 4, "string": None, "cs": "a%d" % i} for i, k in enumerate(kinds)]
            kept.append({"name": name, "ret": ret, "params": params, "slot": slot})
        prefix = "rxdk_%s_" % library
        c_lines = []
        c_lines.append("/* Generated by scripts/gen-sdklibs.py. Do not edit. */")
        c_lines.append("#include <string.h>")
        c_lines.append("")
        c_lines.append("typedef void *(*RxdkDlLoad)(const char *name, int flags, char **err, void *ud);")
        c_lines.append("typedef void *(*RxdkDlSymbol)(void *handle, const char *name, char **err, void *ud);")
        c_lines.append("typedef void *(*RxdkDlClose)(void *handle, void *ud);")
        c_lines.append("extern void *mono_dl_fallback_register(RxdkDlLoad, RxdkDlSymbol, RxdkDlClose, void *);")
        c_lines.append("")
        if any(p["kind"] == "xuid" for fn in kept for p in fn["params"]):
            c_lines.append("#pragma pack(push, 4)")
            c_lines.append("typedef struct { long long id; int flags; } RxdkXuid;")
            c_lines.append("#pragma pack(pop)")
            c_lines.append("")
        retmap = {"void": "void", "ptr": "void *", "f32": "float", "f64": "double", "i64": "long long", "bool": "int", "i32": "int"}
        for fn in kept:
            if "slot" in fn:
                continue
            args = ", ".join("%s a%d" % (c_param(p["kind"]), i) for i, p in enumerate(fn["params"])) or "void"
            c_lines.append("extern %s __attribute__((stdcall)) %s(%s);" % (retmap[fn["ret"]], fn["name"], args))
        c_lines.append("")
        for fn in kept:
            args = ", ".join("%s a%d" % (c_param(p["kind"]), i) for i, p in enumerate(fn["params"])) or "void"
            call = ", ".join("a%d" % i for i in range(len(fn["params"])))
            c_lines.append("static %s %s%s(%s)" % (retmap[fn["ret"]], prefix, fn["name"], args))
            c_lines.append("{")
            target = fn["name"]
            if "slot" in fn:
                types = ", ".join(c_param(p["kind"]) for p in fn["params"])
                target = "((%s (__attribute__((stdcall)) *)(%s))(*(void ***)a0)[%d])" % (retmap[fn["ret"]], types, fn["slot"])
            if fn["ret"] == "void":
                c_lines.append("    %s(%s);" % (target, call))
            else:
                c_lines.append("    return %s(%s);" % (target, call))
            c_lines.append("}")
            c_lines.append("")
        c_lines.append("static void *%s_load(const char *name, int flags, char **err, void *ud)" % library)
        c_lines.append("{")
        c_lines.append("    (void)flags; (void)err; (void)ud;")
        c_lines.append("    if (!name) return NULL;")
        c_lines.append('    if (strcmp(name, "%s") && strcmp(name, "%s.dll")) return NULL;' % (library, library))
        c_lines.append("    return (void *)(unsigned long)%du;" % cookie)
        c_lines.append("}")
        c_lines.append("")
        c_lines.append("static void *%s_symbol(void *handle, const char *name, char **err, void *ud)" % library)
        c_lines.append("{")
        c_lines.append("    (void)err; (void)ud;")
        c_lines.append("    if (handle != (void *)(unsigned long)%du || !name) return NULL;" % cookie)
        for fn in kept:
            c_lines.append('    if (!strcmp(name, "%s%s")) return (void *)&%s%s;' % (prefix, fn["name"], prefix, fn["name"]))
        c_lines.append("    return NULL;")
        c_lines.append("}")
        c_lines.append("")
        c_lines.append("static void *%s_close(void *handle, void *ud)" % library)
        c_lines.append("{")
        c_lines.append("    (void)handle; (void)ud;")
        c_lines.append("    return NULL;")
        c_lines.append("}")
        c_lines.append("")
        c_lines.append("void %s(void)" % register)
        c_lines.append("{")
        c_lines.append("    mono_dl_fallback_register(%s_load, %s_symbol, %s_close, NULL);" % (library, library, library))
        c_lines.append("}")
        c_lines.append("")
        c_path = os.path.join(ROOT, "pal", "src", "bind", library + "_bind.c")
        cs_dir = os.path.join(ROOT, "src", assembly)
        os.makedirs(cs_dir, exist_ok=True)
        cs_path = os.path.join(cs_dir, cls + ".cs")
        libs_path = os.path.join(cs_dir, assembly + ".dll.libs")
        open(c_path, "w", newline="\n").write("\n".join(c_lines))
        open(cs_path, "w", newline="\n").write(emit_cs(library, cls, kept))
        sidecar = [register] + libfile.split()
        open(libs_path, "w", newline="\n").write("\n".join(sidecar) + "\n")
        print("%s: %d wrapped, %d size-mismatch, %d not exported (parsed %d)" % (assembly, len(kept), skipped, missing, len(funcs)))


if __name__ == "__main__":
    main()
