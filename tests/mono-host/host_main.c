/*
 * RXDK-DotNet — minimal Mono embedding host (Phase-1b: load corlib + init).
 * Points Mono at the DVD (where mscorlib.dll is bundled), then mono_jit_init — which loads corlib.
 * With DISABLE_JIT, execution runs on the interpreter.
 */
#include <xtl.h>

typedef struct _MonoDomain MonoDomain;
typedef struct _MonoImage  MonoImage;
typedef struct _MonoAssembly MonoAssembly;
extern void        mono_set_assemblies_path(const char *path);
extern MonoImage  *mono_image_open(const char *fname, int *status);
extern MonoAssembly *mono_assembly_open(const char *filename, int *status);
extern MonoDomain *mono_jit_init(const char *file);
/* Explicit runtime version avoids mono probing the (non-existent) root exe to auto-detect it. */
extern MonoDomain *mono_jit_init_version(const char *root_domain_name, const char *runtime_version);
/* Execution engine: we're interpreter-only (DISABLE_JIT, no AOT). MONO_AOT_MODE_INTERP_ONLY (8)
 * sets mono_use_interpreter=TRUE (so method "compilation" routes to the interp instead of the
 * asserting JIT stub) without requiring AOT. But it also leaves mono_aot_mode != NONE, which makes
 * the assembly-load hook (load_aot_module) probe for AOT images (crashes: no AOT, dl disabled). So
 * after enabling the interp we reset mono_aot_mode back to NONE — the two are separate globals, and
 * load_aot_module only early-returns on NONE while the interp keys off mono_use_interpreter. */
enum { MONO_AOT_MODE_NONE = 0, MONO_AOT_MODE_INTERP_ONLY = 8 };
extern void mono_jit_set_aot_mode(int mode);
extern int  mono_aot_mode; /* MonoAotMode global in mini-runtime.c */

/* Managed execution (invoke a method through the interpreter). */
typedef struct _MonoClass  MonoClass;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoObject MonoObject;
typedef struct _MonoString MonoString;
extern MonoImage  *mono_assembly_get_image(MonoAssembly *assembly);
extern MonoClass  *mono_class_from_name(MonoImage *image, const char *name_space, const char *name);
extern MonoMethod *mono_class_get_method_from_name(MonoClass *klass, const char *name, int param_count);
/* Entry-point lookup: the mini tests are EXEs, so we invoke Main via the assembly's entry token
 * (class name varies: Tests, BuiltinTests, DevirtualizationTests, ...). */
extern unsigned int mono_image_get_entry_point(MonoImage *image);
extern MonoMethod  *mono_get_method(MonoImage *image, unsigned int token, MonoClass *klass);
extern MonoObject *mono_runtime_invoke(MonoMethod *method, void *obj, void **params, MonoObject **exc);
/* Builds the managed string[] from argv (argv[0] is the program name, argv[1..] become Main's args)
 * and invokes Main, returning its int exit code. Used to pass "--time" to the mini-test driver. */
extern int         mono_runtime_run_main(MonoMethod *method, int argc, char *argv[], MonoObject **exc);
extern void       *mono_object_unbox(MonoObject *obj);
extern MonoClass  *mono_object_get_class(MonoObject *obj);
extern const char *mono_class_get_name(MonoClass *klass);
extern MonoString *mono_object_to_string(MonoObject *obj, MonoObject **exc);
/* Managed->native bridge for redirecting managed Console output to the debug serial. */
extern void  mono_add_internal_call(const char *name, const void *method);
extern char *mono_string_to_utf8(MonoString *s);
extern void  mono_free(void *ptr);

/* The single native sink behind RxdkConsole.Write(string) -> managed Console output on serial. */
static void rxdk_console_write(MonoString *s)
{
    char *u;
    if (!s) return;
    u = mono_string_to_utf8(s);
    if (u) { OutputDebugStringA(u); mono_free(u); }
}

/* xboxkrnl: hand control back to the dashboard/firmware. FIRMWARE_REENTRY: 0=Halt, 1=Reboot,
 * 2=QuickReboot. Used as the app's exit path (an Xbox title never "returns" to a shell). */
extern void __attribute__((__stdcall__)) HalReturnToFirmware(unsigned int routine); /* @4 */
#define HalQuickRebootRoutine 2
extern void        mono_trace_set_level_string(const char *value);
extern void        mono_trace_set_mask_string(const char *value);
extern void        mono_trace_set_log_handler(void (*cb)(const char *domain, const char *level, const char *msg, int fatal, void *user), void *user);
extern void        mono_trace_set_print_handler(void (*cb)(const char *string, int is_stdout));

/* Route Mono's log + print output to the debug UART (env-based logging is disabled on Xbox). */
static void rxdk_mono_log(const char *domain, const char *level, const char *msg, int fatal, void *user)
{
    (void)fatal; (void)user;
    OutputDebugStringA("[mono:"); OutputDebugStringA(level ? level : "?"); OutputDebugStringA("] ");
    if (domain) { OutputDebugStringA(domain); OutputDebugStringA(": "); }
    OutputDebugStringA(msg ? msg : "(null)"); OutputDebugStringA("\n");
}
static void rxdk_mono_print(const char *string, int is_stdout) { (void)is_stdout; OutputDebugStringA(string); }
static void rxdk_print_int(const char *label, int v)
{
    char b[16]; int n = 0, neg = 0; unsigned int u;
    OutputDebugStringA(label);
    if (v < 0) { neg = 1; u = (unsigned int)(-v); } else u = (unsigned int)v;
    if (!u) b[n++] = '0'; else { char t[12]; int ti = 0; while (u) { t[ti++] = (char)('0' + u % 10); u /= 10; } while (ti) b[n++] = t[--ti]; }
    b[n] = 0;
    if (neg) OutputDebugStringA("-");
    OutputDebugStringA(b); OutputDebugStringA("\n");
}

/* Load the test assembly and interpret a couple of static methods — the first managed IL executed
 * on the Xbox. Pure arithmetic (Add, Fib) so success is a clean signal the interpreter works. */
static void rxdk_run_managed(void)
{
    int st = 0;
    MonoAssembly *asmb;
    MonoImage *img;
    MonoClass *klass;
    MonoMethod *runall;
    MonoObject *res, *exc;

    /* Register the serial sink for managed Console output before any managed code runs. */
    mono_add_internal_call("RxdkConsole::Write", (const void *)rxdk_console_write);

    OutputDebugStringA("RXDK-DotNet: loading D:\\assy\\Test.dll\n");
    asmb = mono_assembly_open("D:\\assy\\Test.dll", &st);
    if (!asmb) { OutputDebugStringA("RXDK-DotNet: Test.dll load FAILED\n"); return; }
    img = mono_assembly_get_image(asmb);
    klass = mono_class_from_name(img, "", "RxdkTest");
    if (!klass) { OutputDebugStringA("RXDK-DotNet: class RxdkTest not found\n"); return; }
    runall = mono_class_get_method_from_name(klass, "RunAll", 0);
    if (!runall) { OutputDebugStringA("RXDK-DotNet: RunAll not found\n"); return; }

    OutputDebugStringA("RXDK-DotNet: invoking RxdkTest.RunAll() (managed self-test)\n");
    exc = 0;
    res = mono_runtime_invoke(runall, 0, (void **)0, &exc);
    if (exc) { OutputDebugStringA("RXDK-DotNet: RunAll threw an exception\n"); return; }
    rxdk_print_int("RXDK-DotNet: RunAll failures = ", res ? *(int *)mono_object_unbox(res) : -1);
    OutputDebugStringA("RXDK-DotNet: managed self-test complete\n");
}
/* Run one official Mono JIT regression assembly (mini-<name>.dll, built by build-minitests.sh).
 * Each defines `class Tests` with `static int Main(string[])` -> TestDriver.RunTests, returning the
 * number of failed sub-tests. We pass a null string[] (RunTests handles null args). The per-test
 * "Regression tests: N ran, M failed" line comes out on serial via managed Console.WriteLine. */
static int rxdk_run_minitest(const char *name, const char *path)
{
    int st = 0;
    MonoAssembly *asmb;
    MonoImage *img;
    MonoMethod *entry;
    unsigned int tok;
    MonoObject *exc = 0;
    int failed;
    /* argv[0] = program name (skipped by run_main); "--time" enables the driver's per-test timing. */
    char *argv[2]; argv[0] = (char *)name; argv[1] = "--time";

    OutputDebugStringA("\nRXDK-DotNet: === mini test: ");
    OutputDebugStringA(name); OutputDebugStringA(" ===\n");
    asmb = mono_assembly_open(path, &st);
    if (!asmb) { OutputDebugStringA("  load FAILED\n"); return -1; }
    img = mono_assembly_get_image(asmb);
    tok = mono_image_get_entry_point(img);
    if (!tok) { OutputDebugStringA("  no entry point\n"); return -1; }
    entry = mono_get_method(img, tok, 0);
    if (!entry) { OutputDebugStringA("  entry method not found\n"); return -1; }

    failed = mono_runtime_run_main(entry, 2, argv, &exc);
    if (exc) {
        MonoClass *ec = mono_object_get_class(exc);
        const char *en = ec ? mono_class_get_name(ec) : 0;
        MonoObject *sx = 0;
        MonoString *ss;
        OutputDebugStringA("  Main threw: ");
        OutputDebugStringA(en ? en : "(unknown)");
        OutputDebugStringA("\n");
        ss = mono_object_to_string(exc, &sx);
        if (ss && !sx) {
            char *u = mono_string_to_utf8(ss);
            if (u) { OutputDebugStringA("  "); OutputDebugStringA(u); OutputDebugStringA("\n"); mono_free(u); }
        }
        return -1;
    }
    rxdk_print_int("  failed sub-tests = ", failed);
    return failed;
}

static void rxdk_run_all_minitests(void)
{
    static const char *tests[] = {
        "basic",           "D:\\assy\\mini-basic.dll",
        "basic-long",      "D:\\assy\\mini-basic-long.dll",
        "basic-float",     "D:\\assy\\mini-basic-float.dll",
        "basic-math",      "D:\\assy\\mini-basic-math.dll",
        "arrays",          "D:\\assy\\mini-arrays.dll",
        "objects",         "D:\\assy\\mini-objects.dll",
        "exceptions",      "D:\\assy\\mini-exceptions.dll",
        "builtin-types",   "D:\\assy\\mini-builtin-types.dll",
        "devirtualization","D:\\assy\\mini-devirtualization.dll",
    };
    int i, n = (int)(sizeof(tests) / sizeof(tests[0])) / 2;
    int total_failed = 0, files_run = 0, files_err = 0;
    for (i = 0; i < n; ++i) {
        int f = rxdk_run_minitest(tests[i * 2], tests[i * 2 + 1]);
        if (f < 0) files_err++;
        else { files_run++; total_failed += f; }
    }
    OutputDebugStringA("\nRXDK-DotNet: === mini-test summary ===\n");
    rxdk_print_int("  assemblies run   = ", files_run);
    rxdk_print_int("  assemblies error = ", files_err);
    rxdk_print_int("  total failures   = ", total_failed);
}

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

    /* Sanity-probe GetSystemInfo (mono_pagesize depends on it — a 0 page size wrecks the GC).
     * Local decl: SYSTEM_INFO's dwPageSize is the DWORD at offset 4 (after the 4-byte union). */
    {
        extern void __attribute__((__stdcall__)) GetSystemInfo(void *si);
        unsigned long raw[16]; char b[40]; unsigned long ps;
        for (int i = 0; i < 16; ++i) raw[i] = 0;
        GetSystemInfo(raw);
        ps = raw[1]; /* dwPageSize */
        {
            int n=0; const char *p="page="; while(*p)b[n++]=*p++;
            { unsigned long v=ps; char tmp[12]; int t=0;
              if(!v){b[n++]='0';} else { while(v){tmp[t++]=(char)('0'+v%10);v/=10;} while(t)b[n++]=tmp[--t]; } }
            b[n++]='\n'; b[n]=0;
        }
        OutputDebugStringA("RXDK-DotNet: "); OutputDebugStringA(b);
    }

    mono_set_assemblies_path("D:\\assy");

    /* Resolve the corlib P/Invokes into kernel32.dll (e.g. TimeZoneInfo -> GetTimeZoneInformation,
     * for DateTime.Now) to our linked-in implementations, since the Xbox has no dynamic loading. */
    { extern void rxdk_register_pinvoke_fallback(void); rxdk_register_pinvoke_fallback(); }

    /* Verbose assembly-load tracing (env is disabled on Xbox, so set it programmatically) to see
     * exactly why corlib load fails. */
    mono_trace_set_log_handler(rxdk_mono_log, (void *)0);
    mono_trace_set_print_handler(rxdk_mono_print);
    mono_trace_set_level_string("debug");
    mono_trace_set_mask_string("asm");

    mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP_ONLY); /* enable interpreter EE */
    mono_aot_mode = MONO_AOT_MODE_NONE;               /* ...but keep the AOT loader disabled */

    OutputDebugStringA("RXDK-DotNet: calling mono_jit_init_version (loads corlib)\n");
    domain = mono_jit_init_version("rxdk-dotnet", "v4.0.30319");
    OutputDebugStringA(domain ? "RXDK-DotNet: mono_jit_init OK -- corlib loaded!\n"
                              : "RXDK-DotNet: mono_jit_init returned NULL\n");

    if (domain) {
        rxdk_run_managed();
        rxdk_run_all_minitests();
    }

    /* Let the debug UART drain, then hand control back to the dashboard (an Xbox title exits by
     * returning to firmware rather than falling off the end of main). */
    OutputDebugStringA("RXDK-DotNet: exiting -> HalReturnToFirmware(QuickReboot)\n");
    Sleep(2000);
    HalReturnToFirmware(HalQuickRebootRoutine);
    for (;;) { }   /* unreachable */
}
