/*
 * RXDK-DotNet — cdecl shims for Win32 APIs that the SDK declares __declspec(dllimport) (Sleep,
 * GetTickCount) but Mono references undecorated (cdecl). They can't be defined in any TU that
 * includes <xtl.h> (the dllimport decl forbids it), so this file includes NO SDK headers — its
 * definitions get plain cdecl linkage (_GetTickCount / _Sleep), matching Mono's references. Kernel
 * imports are declared here with their real stdcall decoration so they resolve against libkernel.
 */

/* xboxkrnl exports (declared with matching calling convention / decoration). */
extern volatile unsigned long KeTickCount;                 /* _KeTickCount (data) */
extern long __attribute__((__stdcall__))
KeDelayExecutionThread(int WaitMode, int Alertable, long long *Interval);  /* _KeDelayExecutionThread@12 */

unsigned long GetTickCount(void)
{
    return KeTickCount;
}

void Sleep(unsigned long ms)
{
    /* relative negative 100 ns interval on the kernel timer */
    long long interval = -((long long)ms * 10000);
    KeDelayExecutionThread(0 /*KernelMode*/, 0 /*not alertable*/, &interval);
}

/* The Xbox has no process environment. eglib's g_getenv (symbol monoeg_g_getenv) otherwise routes
 * to GetEnvironmentVariableW and returned garbage-as-found for every variable, so Mono spuriously
 * acted on MONO_PATH/MONO_GC_PARAMS/DUMP_CROSS_OFFSETS/... during init. Override to "always unset"
 * (these loose defs win the --allow-multiple-definition race over libeglib). */
char *monoeg_g_getenv(const char *name) { (void)name; return (char *)0; }
int   monoeg_g_hasenv(const char *name) { (void)name; return 0; }

/* RXDK's WIDE file APIs fail on D:\ paths (verified: GetFileAttributesA works, GetFileAttributesW
 * returns INVALID). Mono is a UNICODE build so it calls the W variants. Override them with W->A
 * thunks (assembly paths are ASCII). These loose defs win the --allow-multiple-definition race. */
extern unsigned long __attribute__((__stdcall__)) GetFileAttributesA(const char *path); /* @4 */
extern void *__attribute__((__stdcall__)) CreateFileA(const char *name, unsigned long access, unsigned long share,
                                                      void *sa, unsigned long disp, unsigned long flags, void *tmpl); /* @28 */
static void rxdk_w2a(const unsigned short *w, char *a, int n)
{
    int i = 0;
    if (w) for (; i < n - 1 && w[i]; ++i) a[i] = (char)w[i];
    a[i] = 0;
}
unsigned long __attribute__((__stdcall__)) GetFileAttributesW(const unsigned short *path)
{
    char a[520]; rxdk_w2a(path, a, 520); return GetFileAttributesA(a);
}
void *__attribute__((__stdcall__)) CreateFileW(const unsigned short *name, unsigned long access, unsigned long share,
                                               void *sa, unsigned long disp, unsigned long flags, void *tmpl)
{
    char a[520]; rxdk_w2a(name, a, 520); return CreateFileA(a, access, share, sa, disp, flags, tmpl);
}
extern void *__attribute__((__stdcall__)) CreateFileMappingA(void *hFile, void *sa, unsigned long protect,
                                                             unsigned long maxHigh, unsigned long maxLow, const char *name); /* @24 */
void *__attribute__((__stdcall__)) CreateFileMappingW(void *hFile, void *sa, unsigned long protect,
                                                      unsigned long maxHigh, unsigned long maxLow, const unsigned short *name)
{
    char a[520];
    const char *n = (const char *)0;
    if (name) { rxdk_w2a(name, a, 520); n = a; }
    return CreateFileMappingA(hFile, sa, protect, maxHigh, maxLow, n);
}

/* Win32 APIs Mono references undecorated (SDK lacks the exact variant). Defined cdecl here to match.
 * CreateSemaphoreW is real (threads/GC need it) -> the SDK's ANSI CreateSemaphoreA (a stdcall
 * libxapi export). The rest are unused-feature stubs. */
extern void *__attribute__((__stdcall__)) CreateSemaphoreA(void *sa, long initial, long max, const char *name); /* _CreateSemaphoreA@16 */
void *CreateSemaphoreW(void *sa, long initial, long max, const unsigned short *name) { (void)name; return CreateSemaphoreA(sa, initial, max, (const char *)0); }
void *GetModuleHandle(const void *name) { (void)name; return (void *)0x00010000; } /* image base as a token */
int   GetConsoleMode(void *h, unsigned long *mode) { (void)h; (void)mode; return 0; } /* no console */
int   SetThreadContext(void *thread, const void *ctx) { (void)thread; (void)ctx; return 0; }
int   CancelSynchronousIo(void *thread) { (void)thread; return 0; }
int   ImpersonateLoggedOnUser(void *token) { (void)token; return 0; }
int   RevertToSelf(void) { return 1; }
void  IoCompleteRequest(void) {} /* kernel DDK — unreachable in our usermode paths */

unsigned long SetErrorMode(unsigned long mode) { (void)mode; return 0; } /* no error dialogs on Xbox */

/* No Win32 message queue on Xbox — treat the message-wait as a plain object wait. */
extern unsigned long __attribute__((__stdcall__))
WaitForMultipleObjects(unsigned long count, const void *handles, int waitAll, unsigned long ms); /* _WaitForMultipleObjects@16 */
unsigned long MsgWaitForMultipleObjectsEx(unsigned long count, const void *handles,
                                          unsigned long ms, unsigned long wakeMask, unsigned long flags)
{
    (void)wakeMask;
    return WaitForMultipleObjects(count, handles, (flags & 0x0001 /*MWMO_WAITALL*/) ? 1 : 0, ms);
}

