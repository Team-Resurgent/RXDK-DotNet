/*
 * RXDK-DotNet — minimal Mono embedding host (Phase-1b: load corlib + init).
 * Points Mono at the DVD (where mscorlib.dll is bundled), then mono_jit_init — which loads corlib.
 * With DISABLE_JIT, execution runs on the interpreter.
 */
#include <xtl.h>

typedef struct _MonoDomain MonoDomain;
extern void        mono_set_assemblies_path(const char *path);
extern MonoDomain *mono_jit_init(const char *file);
extern void        mono_trace_set_level_string(const char *value);
extern void        mono_trace_set_mask_string(const char *value);
static void test_path(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    OutputDebugStringA(p);
    OutputDebugStringA(a == 0xFFFFFFFF ? "  -> INVALID\n" : "  -> FOUND\n");
}

void __cdecl main(void)
{
    MonoDomain *domain;
    OutputDebugStringA("RXDK-DotNet: mono host starting\n");

    /* D: is already the title drive (DVD when booted from disc; the title's dir from HDD) — don't
     * force-remap it. Just probe which path convention GetFileAttributes actually resolves. */
    OutputDebugStringA("RXDK-DotNet: file-access probe:\n");
    test_path("D:\\mscorlib.dll");
    test_path("D:\\assy\\mscorlib.dll");
    test_path("\\Device\\CdRom0\\assy\\mscorlib.dll");
    test_path("\\??\\D:\\assy\\mscorlib.dll");
    test_path("D:\\assy");
    test_path("D:\\");

    mono_set_assemblies_path("D:\\assy");

    /* Verbose assembly-load tracing (env is disabled on Xbox, so set it programmatically) to see
     * exactly why corlib load fails. */
    mono_trace_set_level_string("debug");
    mono_trace_set_mask_string("asm");

    OutputDebugStringA("RXDK-DotNet: calling mono_jit_init (loads corlib)\n");
    domain = mono_jit_init("rxdk-dotnet");
    OutputDebugStringA(domain ? "RXDK-DotNet: mono_jit_init OK -- corlib loaded!\n"
                              : "RXDK-DotNet: mono_jit_init returned NULL\n");

    for (;;)
        Sleep(1000);
}
