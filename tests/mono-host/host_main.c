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
extern MonoImage  *mono_assembly_get_image(MonoAssembly *assembly);
extern MonoClass  *mono_class_from_name(MonoImage *image, const char *name_space, const char *name);
extern MonoMethod *mono_class_get_method_from_name(MonoClass *klass, const char *name, int param_count);
extern MonoObject *mono_runtime_invoke(MonoMethod *method, void *obj, void **params, MonoObject **exc);
extern void       *mono_object_unbox(MonoObject *obj);

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
    MonoMethod *add, *fib;
    MonoObject *res, *exc;
    int a, b;
    void *args[2];

    OutputDebugStringA("RXDK-DotNet: loading D:\\assy\\Test.dll\n");
    asmb = mono_assembly_open("D:\\assy\\Test.dll", &st);
    if (!asmb) { OutputDebugStringA("RXDK-DotNet: Test.dll load FAILED\n"); return; }
    img = mono_assembly_get_image(asmb);
    klass = mono_class_from_name(img, "", "RxdkTest");
    if (!klass) { OutputDebugStringA("RXDK-DotNet: class RxdkTest not found\n"); return; }
    add = mono_class_get_method_from_name(klass, "Add", 2);
    fib = mono_class_get_method_from_name(klass, "Fib", 1);
    if (!add || !fib) { OutputDebugStringA("RXDK-DotNet: method not found\n"); return; }

    a = 20; b = 22; args[0] = &a; args[1] = &b; exc = 0;
    OutputDebugStringA("RXDK-DotNet: invoking RxdkTest.Add(20, 22)\n");
    res = mono_runtime_invoke(add, 0, args, &exc);
    if (exc) { OutputDebugStringA("RXDK-DotNet: Add threw an exception\n"); return; }
    rxdk_print_int("RXDK-DotNet: Add returned = ", *(int *)mono_object_unbox(res));

    a = 20; args[0] = &a; exc = 0;
    OutputDebugStringA("RXDK-DotNet: invoking RxdkTest.Fib(20)\n");
    res = mono_runtime_invoke(fib, 0, args, &exc);
    if (exc) { OutputDebugStringA("RXDK-DotNet: Fib threw an exception\n"); return; }
    rxdk_print_int("RXDK-DotNet: Fib(20) = ", *(int *)mono_object_unbox(res));
    OutputDebugStringA("RXDK-DotNet: managed execution OK\n");
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

    if (domain)
        rxdk_run_managed();

    /* Let the debug UART drain, then hand control back to the dashboard (an Xbox title exits by
     * returning to firmware rather than falling off the end of main). */
    OutputDebugStringA("RXDK-DotNet: exiting -> HalReturnToFirmware(QuickReboot)\n");
    Sleep(2000);
    HalReturnToFirmware(HalQuickRebootRoutine);
    for (;;) { }   /* unreachable */
}
