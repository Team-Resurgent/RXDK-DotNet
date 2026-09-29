# Emit the XGraphics math surface.
# Short header routines and the degree/radian macros are C#.
# The exported XG* functions stay calls into libxgraphics.
import re
from pathlib import Path

HEADER = Path(r"C:\ProgramData\RXDK\sdk\include\xgmath.h")
ROOT = Path(__file__).resolve().parents[1]
CS = ROOT / "src" / "Rxdk.Graphics" / "Math.cs"
C = ROOT / "pal" / "src" / "bind" / "xgmath_bind.c"

PREFIX = (
    ("XGVec2", "Vector2"),
    ("XGVec3", "Vector3"),
    ("XGVec4", "Vector4"),
    ("XGMatrixf", "Matrix"),
    ("XGMatrix", "Matrix"),
    ("XGQuaternion", "Quaternion"),
    ("XGPlane", "Plane"),
    ("XGColor", "Color4"),
)

KIND = {
    "XGVECTOR2": ("Vector2", 8),
    "XGVECTOR3": ("Vector3", 12),
    "XGVECTOR4": ("Vector4", 16),
    "XGQUATERNION": ("Quaternion", 16),
    "XGPLANE": ("Plane", 16),
    "XGCOLOR": ("Color4", 16),
    "XGMATRIX": ("Matrix4", 64),
    "D3DVIEWPORT8": ("Viewport", 24),
}

COMPONENTS = {
    "Vector2": ("X", "Y"),
    "Vector3": ("X", "Y", "Z"),
    "Vector4": ("X", "Y", "Z", "W"),
    "Quaternion": ("X", "Y", "Z", "W"),
    "Plane": ("A", "B", "C", "D"),
    "Color4": ("R", "G", "B", "A"),
}


def method_name(fn):
    for prefix, _cls in PREFIX:
        if fn.startswith(prefix):
            name = fn[len(prefix):]
            if name.startswith("f") and len(name) > 1 and name[1].isupper():
                name = name[1:]
            return name
    return fn


def owner(fn):
    for prefix, cls in PREFIX:
        if fn.startswith(prefix):
            return cls
    return "Matrix"


def cs_param(name):
    if name.startswith("p") and len(name) > 1 and name[1].isupper():
        name = name[1:]
    name = name[0].lower() + name[1:]
    if name in ("out", "ref", "in", "float", "params", "object", "string"):
        name += "Value"
    return name


def parse_param(raw):
    text = " ".join(raw.replace("CONST", "").split())
    match = re.match(r"(FLOAT|BOOL|D3DVIEWPORT8|XG\w+)\s*(\*)?\s*(\w+)$", text)
    if not match:
        raise SystemExit("param: " + raw)
    base, star, name = match.group(1), match.group(2), match.group(3)
    const = raw.strip().startswith("CONST")
    if star and base == "FLOAT":
        return {"kind": "float_out", "name": cs_param(name), "bytes": 4}
    if star:
        cs, nbytes = KIND[base]
        # pOut is the result. Other pV* pointers are inputs; the Vec2
        # barycentric header omits CONST on pV3.
        kind = "in" if const or name.startswith("pV") else "out"
        return {
            "kind": kind,
            "name": cs_param(name),
            "cs": cs,
            "bytes": nbytes,
        }
    if base == "FLOAT":
        return {"kind": "float", "name": cs_param(name), "bytes": 4}
    if base == "BOOL":
        return {"kind": "bool", "name": cs_param(name), "bytes": 4}
    raise SystemExit("type: " + raw)


def parse_functions(text):
    text = text.split('#include "xgmath.inl"')[0]
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    found = []
    pat = re.compile(
        r"(FLOAT|BOOL|void|XG\w+\*)\s+(WINAPI\s+)?(XG(?:Vec2|Vec3|Vec4|Matrix|Quaternion|Plane|Color)\w+)\s*\((.*?)\)\s*;",
        re.S,
    )
    for ret, winapi, name, params in pat.findall(text):
        plist = []
        body = " ".join(params.split()).strip()
        if body and body != "void":
            for part in split_commas(body):
                plist.append(parse_param(part))
        seen = {}
        for p in plist:
            base = p["name"]
            n = base
            i = 2
            while n in seen:
                n = base + str(i)
                i += 1
            seen[n] = True
            p["name"] = n
        found.append({"ret": ret, "export": bool(winapi), "name": name, "params": plist})
    return found


def split_commas(text):
    parts, buf = [], ""
    for ch in text:
        if ch == ",":
            parts.append(buf.strip())
            buf = ""
        else:
            buf += ch
    if buf.strip():
        parts.append(buf.strip())
    return parts


def component_list(cls):
    return COMPONENTS[cls]


def ported_methods():
    blocks = {name: [] for name in (
        "Vector2", "Vector3", "Vector4", "Matrix", "Quaternion", "Plane", "Color4")}

    def vec(cls):
        fields = component_list(cls)
        dot = " + ".join("a.{0} * b.{0}".format(f) for f in fields)
        add = ", ".join("a.{0} + b.{0}".format(f) for f in fields)
        sub = ", ".join("a.{0} - b.{0}".format(f) for f in fields)
        scale = ", ".join("v.{0} * s".format(f) for f in fields)
        lerp = ", ".join("(a.{0} + s * (b.{0} - a.{0}))".format(f) for f in fields)
        mn = ", ".join("a.{0} < b.{0} ? a.{0} : b.{0}".format(f) for f in fields)
        mx = ", ".join("a.{0} > b.{0} ? a.{0} : b.{0}".format(f) for f in fields)
        sq = " + ".join("v.{0} * v.{0}".format(f) for f in fields)
        ctor = "new {0}(".format(cls)
        blocks[cls].append("public static float LengthSq({0} v) {{ return {1}; }}".format(cls, sq))
        blocks[cls].append("public static float Length({0} v) {{ return (float)Math.Sqrt(LengthSq(v)); }}".format(cls))
        blocks[cls].append("public static float Dot({0} a, {0} b) {{ return {1}; }}".format(cls, dot))
        blocks[cls].append("public static {0} Add({0} a, {0} b) {{ return {1}{2}); }}".format(cls, ctor, add))
        blocks[cls].append("public static {0} Subtract({0} a, {0} b) {{ return {1}{2}); }}".format(cls, ctor, sub))
        blocks[cls].append("public static {0} Scale({0} v, float s) {{ return {1}{2}); }}".format(cls, ctor, scale))
        blocks[cls].append("public static {0} Lerp({0} a, {0} b, float s) {{ return {1}{2}); }}".format(cls, ctor, lerp))
        blocks[cls].append("public static {0} Minimize({0} a, {0} b) {{ return {1}{2}); }}".format(cls, ctor, mn))
        blocks[cls].append("public static {0} Maximize({0} a, {0} b) {{ return {1}{2}); }}".format(cls, ctor, mx))

    vec("Vector2")
    vec("Vector3")
    vec("Vector4")
    blocks["Vector2"].append("public static float Ccw(Vector2 a, Vector2 b) { return a.X * b.Y - a.Y * b.X; }")
    blocks["Vector3"].append(
        "public static Vector3 Cross(Vector3 a, Vector3 b) { return new Vector3("
        "a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X); }")
    blocks["Quaternion"].append("public static float LengthSq(Quaternion v) { return v.X * v.X + v.Y * v.Y + v.Z * v.Z + v.W * v.W; }")
    blocks["Quaternion"].append("public static float Length(Quaternion v) { return (float)Math.Sqrt(LengthSq(v)); }")
    blocks["Quaternion"].append("public static float Dot(Quaternion a, Quaternion b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }")
    blocks["Quaternion"].append("public static Quaternion Identity() { return new Quaternion(0f, 0f, 0f, 1f); }")
    blocks["Quaternion"].append("public static bool IsIdentity(Quaternion q) { return q.X == 0f && q.Y == 0f && q.Z == 0f && q.W == 1f; }")
    blocks["Quaternion"].append("public static Quaternion Conjugate(Quaternion q) { return new Quaternion(-q.X, -q.Y, -q.Z, q.W); }")
    blocks["Plane"].append("public static float Dot(Plane p, Vector4 v) { return p.A * v.X + p.B * v.Y + p.C * v.Z + p.D * v.W; }")
    blocks["Plane"].append("public static float DotCoord(Plane p, Vector3 v) { return p.A * v.X + p.B * v.Y + p.C * v.Z + p.D; }")
    blocks["Plane"].append("public static float DotNormal(Plane p, Vector3 v) { return p.A * v.X + p.B * v.Y + p.C * v.Z; }")
    blocks["Color4"].append("public static Color4 Negative(Color4 c) { return new Color4(1f - c.R, 1f - c.G, 1f - c.B, c.A); }")
    blocks["Color4"].append("public static Color4 Add(Color4 a, Color4 b) { return new Color4(a.R + b.R, a.G + b.G, a.B + b.B, a.A + b.A); }")
    blocks["Color4"].append("public static Color4 Subtract(Color4 a, Color4 b) { return new Color4(a.R - b.R, a.G - b.G, a.B - b.B, a.A - b.A); }")
    blocks["Color4"].append("public static Color4 Scale(Color4 c, float s) { return new Color4(c.R * s, c.G * s, c.B * s, c.A * s); }")
    blocks["Color4"].append("public static Color4 Modulate(Color4 a, Color4 b) { return new Color4(a.R * b.R, a.G * b.G, a.B * b.B, a.A * b.A); }")
    blocks["Color4"].append("public static Color4 Lerp(Color4 a, Color4 b, float s) { return new Color4(a.R + s * (b.R - a.R), a.G + s * (b.G - a.G), a.B + s * (b.B - a.B), a.A + s * (b.A - a.A)); }")
    blocks["Matrix"].append(
        "public static Matrix4 Identity()\n        {\n"
        "            Matrix4 m = new Matrix4();\n"
        "            m.M11 = 1f; m.M22 = 1f; m.M33 = 1f; m.M44 = 1f;\n"
        "            return m;\n        }")
    blocks["Matrix"].append(
        "public static bool IsIdentity(Matrix4 m)\n        {\n"
        "            return m.M11 == 1f && m.M12 == 0f && m.M13 == 0f && m.M14 == 0f &&\n"
        "                m.M21 == 0f && m.M22 == 1f && m.M23 == 0f && m.M24 == 0f &&\n"
        "                m.M31 == 0f && m.M32 == 0f && m.M33 == 1f && m.M34 == 0f &&\n"
        "                m.M41 == 0f && m.M42 == 0f && m.M43 == 0f && m.M44 == 1f;\n        }")
    return blocks


def emit():
    funcs = parse_functions(HEADER.read_text(encoding="utf-8", errors="replace"))
    exported = [f for f in funcs if f["export"]]
    blocks = ported_methods()
    c_decls, c_funcs, c_syms, native_decls = emit_c(exported)
    cs = emit_cs(blocks, native_decls)
    CS.write_text(cs, encoding="utf-8", newline="\n")
    C.write_text(emit_c_file(c_decls, c_funcs, c_syms), encoding="utf-8", newline="\n")
    print("functions", len(funcs), "exported", len(exported))
    print("wrote", CS)
    print("wrote", C)


def emit_c(exported):
    decls, funcs, syms, natives = [], [], [], []
    for fn in exported:
        params = list(fn["params"])
        primary = None
        if fn["ret"].endswith("*") and params and params[0]["kind"] == "out":
            primary = params.pop(0)
        ext_parts = []
        for p in fn["params"]:
            if p["kind"] == "float":
                ext_parts.append("float")
            elif p["kind"] == "bool":
                ext_parts.append("int")
            elif p["kind"] == "float_out":
                ext_parts.append("float *")
            elif p["kind"] == "in":
                ext_parts.append("const void *")
            else:
                ext_parts.append("void *")
        if fn["ret"].endswith("*"):
            ext_ret = "void *"
        elif fn["ret"] == "FLOAT":
            ext_ret = "float"
        elif fn["ret"] == "BOOL":
            ext_ret = "int"
        else:
            ext_ret = "void"
        decls.append("extern {0} __attribute__((stdcall)) {1}({2});".format(
            ext_ret, fn["name"], ", ".join(ext_parts) if ext_parts else "void"))

        sig = []
        lines = []
        call = []
        n = 0
        if primary:
            sig.append("void *o")
            lines.append("    __attribute__((aligned(16))) unsigned char bo[{0}];".format(max(primary["bytes"], 16)))
            call.append("bo")
        for p in params:
            if p["kind"] == "float":
                sig.append("float " + p["name"])
                call.append(p["name"])
            elif p["kind"] == "float_out":
                sig.append("float *" + p["name"])
                call.append(p["name"])
            elif p["kind"] == "in":
                sig.append("const void *" + p["name"])
                slot = "b{0}".format(n)
                n += 1
                lines.append("    __attribute__((aligned(16))) unsigned char {0}[{1}];".format(slot, max(p["bytes"], 16)))
                lines.append("    if ({0}) memcpy({1}, {0}, {2}); else memset({1}, 0, {2});".format(p["name"], slot, p["bytes"]))
                call.append(slot)
            elif p["kind"] == "out":
                sig.append("void *" + p["name"])
                slot = "b{0}".format(n)
                n += 1
                lines.append("    __attribute__((aligned(16))) unsigned char {0}[{1}];".format(slot, max(p["bytes"], 16)))
                call.append(slot)
                lines.append("    /* copy {0} after */".format(p["name"]))
                p["slot"] = slot
        cname = "rxdk_xm_" + fn["name"]
        invocation = "{0}({1})".format(fn["name"], ", ".join(call))
        if fn["ret"] == "FLOAT":
            lines.append("    return {0};".format(invocation))
            ret_c = "float"
        elif fn["ret"] == "BOOL":
            lines.append("    return {0};".format(invocation))
            ret_c = "int"
        else:
            lines.append("    {0};".format(invocation))
            ret_c = "void"
        if primary:
            lines.append("    if (o) memcpy(o, bo, {0});".format(primary["bytes"]))
        for p in params:
            if p["kind"] == "out":
                lines.append("    if ({0}) memcpy({0}, {1}, {2});".format(p["name"], p["slot"], p["bytes"]))
        funcs.append("static {0} {1}({2})\n{{\n{3}\n}}".format(
            ret_c, cname, ", ".join(sig) if sig else "void", "\n".join(lines)))
        syms.append('    if (!strcmp(name, "{0}")) return (void *)&{0};'.format(cname))

        native_sig = []
        call_native = []
        pub = []
        prep = []
        if primary:
            native_sig.append("ref {0} o".format(primary["cs"]))
            call_native.append("ref o")
            prep.append("{0} o = new {0}();".format(primary["cs"]))
        for p in params:
            if p["kind"] == "float":
                native_sig.append("float " + p["name"])
                call_native.append(p["name"])
                pub.append("float " + p["name"])
            elif p["kind"] == "float_out":
                native_sig.append("out float " + p["name"])
                call_native.append("out " + p["name"])
                pub.append("out float " + p["name"])
            elif p["kind"] == "in":
                native_sig.append("ref {0} {1}".format(p["cs"], p["name"]))
                call_native.append("ref " + p["name"])
                pub.append("{0} {1}".format(p["cs"], p["name"]))
            elif p["kind"] == "out":
                native_sig.append("out {0} {1}".format(p["cs"], p["name"]))
                call_native.append("out " + p["name"])
                pub.append("out {0} {1}".format(p["cs"], p["name"]))
        if primary:
            body = "            {0}\n            Native.{1}({2});\n            return o;".format(
                " ".join(prep), fn["name"], ", ".join(call_native))
            cs_ret = primary["cs"]
        elif fn["ret"] == "FLOAT":
            body = "            return Native.{0}({1});".format(fn["name"], ", ".join(call_native))
            cs_ret = "float"
        elif fn["ret"] == "BOOL":
            body = "            return Native.{0}({1}) != 0;".format(fn["name"], ", ".join(call_native))
            cs_ret = "bool"
        else:
            body = "            Native.{0}({1});".format(fn["name"], ", ".join(call_native))
            cs_ret = "void"
        # attach csharp onto a side channel via natives list of (owner, method text, native decl)
        method = "public static {0} {1}({2})\n        {{\n{3}\n        }}".format(
            cs_ret, method_name(fn["name"]), ", ".join(pub), body)
        if fn["ret"] == "FLOAT":
            nd = "public static extern float {0}({1});".format(fn["name"], ", ".join(native_sig))
        elif fn["ret"] == "BOOL":
            nd = "public static extern int {0}({1});".format(fn["name"], ", ".join(native_sig))
        else:
            nd = "public static extern void {0}({1});".format(fn["name"], ", ".join(native_sig))
        natives.append((owner(fn["name"]), method, nd, cname))
    return decls, funcs, syms, natives


def emit_cs(blocks, natives):
    for owner_name, method, _nd, _cn in natives:
        blocks[owner_name].append(method)
    native_lines = []
    for _owner, _method, nd, cname in natives:
        native_lines.append(
            '            [DllImport("xgraphics", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true, EntryPoint = "{0}")]\n            {1}'.format(
                cname, nd))

    def struct(name, fields, ctor_args):
        body = "\n        ".join(blocks[name])
        assigns = " ".join("{0} = {1};".format(f, f.lower()) for f in fields)
        args = ", ".join("float " + f.lower() for f in fields)
        return (
            "    [StructLayout(LayoutKind.Sequential)]\n"
            "    public struct " + name + "\n    {\n"
            "        public float " + ", ".join(fields) + ";\n"
            "        public " + name + "(" + args + ") { " + assigns + " }\n"
            "        " + body + "\n    }\n"
        )

    parts = []
    parts.append(struct("Vector2", ("X", "Y"), None))
    parts.append(struct("Vector3", ("X", "Y", "Z"), None))
    parts.append(struct("Vector4", ("X", "Y", "Z", "W"), None))
    parts.append(struct("Quaternion", ("X", "Y", "Z", "W"), None))
    parts.append(struct("Plane", ("A", "B", "C", "D"), None))
    parts.append(struct("Color4", ("R", "G", "B", "A"), None))
    matrix_body = "\n        ".join(blocks["Matrix"])
    parts.append("    public static class Matrix\n    {\n        " + matrix_body + "\n    }\n")
    text = """// Generated by scripts/gen-xgmath.py. Degree and radian conversion and the
// short vector, matrix, quaternion, plane, and color routines are C#. The
// exported XGraphics functions call libxgraphics.
using System;
using System.Runtime.InteropServices;

namespace Rxdk
{
    public static class MathHelper
    {
        public const float Pi = 3.141592654f;
        public const float OneOverPi = 0.318309886f;

        public static float ToRadian(float degree)
        {
            return degree * (Pi / 180.0f);
        }

        public static float ToDegree(float radian)
        {
            return radian * (180.0f / Pi);
        }
    }

"""
    text += "\n".join(parts)
    text += """
    static class XgNative
    {
"""
    # rewrite Native. to XgNative. in text? methods say Native.
    text = text.replace("Native.", "XgNative.")
    text += "\n".join(native_lines)
    text += """
    }
}
"""
    return text


def emit_c_file(decls, funcs, syms):
    return """/* Generated by scripts/gen-xgmath.py. Exported XGraphics math only.
 * The short routines live in C# (Math.cs). */
#include <string.h>

typedef void *(*RxdkDlLoad)(const char *name, int flags, char **err, void *ud);
typedef void *(*RxdkDlSymbol)(void *handle, const char *name, char **err, void *ud);
typedef void *(*RxdkDlClose)(void *handle, void *ud);
extern void *mono_dl_fallback_register(RxdkDlLoad, RxdkDlSymbol, RxdkDlClose, void *);

""" + "\n".join(decls) + "\n\n" + "\n\n".join(funcs) + """

static void *xg_load(const char *name, int flags, char **err, void *ud)
{
    (void)flags; (void)err; (void)ud;
    if (name && (strcmp(name, "xgraphics") == 0 || strcmp(name, "xgraphics.dll") == 0))
        return (void *)(size_t)0x58474D41;
    return NULL;
}

static void *xg_symbol(void *handle, const char *name, char **err, void *ud)
{
    (void)handle; (void)err; (void)ud;
    if (!name)
        return NULL;
""" + "\n".join(syms) + """
    return NULL;
}

static void *xg_close(void *handle, void *ud)
{
    (void)handle; (void)ud;
    return NULL;
}

void rxdk_bind_xgmath_register(void)
{
    mono_dl_fallback_register(xg_load, xg_symbol, xg_close, NULL);
}
"""


if __name__ == "__main__":
    emit()
