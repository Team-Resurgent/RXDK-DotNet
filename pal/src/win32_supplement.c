/*
 * RXDK-DotNet — implementations of the Vista-era Win32 primitives that win32_supplement.h declares,
 * emulated over the Windows-2000-era surface the Xbox kernel / RXDK libxapi actually provide
 * (CRITICAL_SECTION, events, CreateThread). Correct enough for the interpreter bring-up; not tuned.
 *
 * TODO: SRWLOCK is emulated as a plain recursive-ish lock (shared == exclusive) — fine for
 * correctness, not for reader concurrency. The condition variable is a simple event-based waiter.
 */
#include <xtl.h>
#include <stdlib.h>

#include "rxdk/win32_supplement.h"
#include <psapi.h>   /* PROCESS_MEMORY_COUNTERS (our compat shim) */

/* ---- lazy-init guard (startup is effectively single-threaded) -------------------------------- */
static CRITICAL_SECTION g_init_cs;
static int g_init_cs_ready;
static void init_guard_enter(void)
{
    if (!g_init_cs_ready) { InitializeCriticalSection(&g_init_cs); g_init_cs_ready = 1; }
    EnterCriticalSection(&g_init_cs);
}
static void init_guard_leave(void) { LeaveCriticalSection(&g_init_cs); }

/* ---- SRWLOCK over CRITICAL_SECTION ----------------------------------------------------------- */
static CRITICAL_SECTION *srw_cs(PSRWLOCK lock)
{
    if (!lock->Ptr) {
        init_guard_enter();
        if (!lock->Ptr) {
            CRITICAL_SECTION *cs = (CRITICAL_SECTION *)malloc(sizeof(*cs));
            InitializeCriticalSection(cs);
            lock->Ptr = cs;
        }
        init_guard_leave();
    }
    return (CRITICAL_SECTION *)lock->Ptr;
}
void __stdcall InitializeSRWLock(PSRWLOCK lock) { lock->Ptr = NULL; (void)srw_cs(lock); }
void __stdcall AcquireSRWLockExclusive(PSRWLOCK lock) { EnterCriticalSection(srw_cs(lock)); }
void __stdcall ReleaseSRWLockExclusive(PSRWLOCK lock) { LeaveCriticalSection(srw_cs(lock)); }
void __stdcall AcquireSRWLockShared(PSRWLOCK lock)    { EnterCriticalSection(srw_cs(lock)); }
void __stdcall ReleaseSRWLockShared(PSRWLOCK lock)    { LeaveCriticalSection(srw_cs(lock)); }

/* ---- CONDITION_VARIABLE over an auto-reset event --------------------------------------------- */
static HANDLE cv_event(PCONDITION_VARIABLE cv)
{
    if (!cv->Ptr) {
        init_guard_enter();
        if (!cv->Ptr)
            cv->Ptr = CreateEventA(NULL, FALSE /*auto-reset*/, FALSE /*non-signalled*/, NULL);
        init_guard_leave();
    }
    return (HANDLE)cv->Ptr;
}
void __stdcall InitializeConditionVariable(PCONDITION_VARIABLE cv) { cv->Ptr = NULL; (void)cv_event(cv); }

int __stdcall SleepConditionVariableCS(PCONDITION_VARIABLE cv, void *cs, unsigned long ms)
{
    HANDLE ev = cv_event(cv);
    DWORD r;
    LeaveCriticalSection((CRITICAL_SECTION *)cs);
    r = WaitForSingleObject(ev, ms);
    EnterCriticalSection((CRITICAL_SECTION *)cs);
    return r == WAIT_OBJECT_0 ? 1 : 0;
}
int __stdcall SleepConditionVariableSRW(PCONDITION_VARIABLE cv, PSRWLOCK lock, unsigned long ms, unsigned long flags)
{
    (void)flags;
    return SleepConditionVariableCS(cv, srw_cs(lock), ms);
}
void __stdcall WakeConditionVariable(PCONDITION_VARIABLE cv)    { SetEvent(cv_event(cv)); }
void __stdcall WakeAllConditionVariable(PCONDITION_VARIABLE cv) { SetEvent(cv_event(cv)); /* wakes one; loop-callers re-check */ }

/* ---- system info / memory -------------------------------------------------------------------- */
void __stdcall GetSystemInfo(LPSYSTEM_INFO si)
{
    if (!si) return;
    si->dwOemId = 0;
    si->dwPageSize = 4096;
    si->lpMinimumApplicationAddress = (void *)0x00010000;
    si->lpMaximumApplicationAddress = (void *)0x03FFFFFF; /* 64 MB retail */
    si->dwActiveProcessorMask = 1;
    si->dwNumberOfProcessors = 1;
    si->dwProcessorType = 586;
    si->dwAllocationGranularity = 0x10000;
    si->wProcessorLevel = 6;      /* PIII */
    si->wProcessorRevision = 0;
}

int __stdcall GlobalMemoryStatusEx(LPMEMORYSTATUSEX ms)
{
    if (!ms) return 0;
    /* Coarse figures for the retail 64 MB console; refine over MmQueryStatistics later. */
    ms->dwMemoryLoad = 50;
    ms->ullTotalPhys        = 64ULL * 1024 * 1024;
    ms->ullAvailPhys        = 32ULL * 1024 * 1024;
    ms->ullTotalPageFile    = ms->ullTotalPhys;
    ms->ullAvailPageFile    = ms->ullAvailPhys;
    ms->ullTotalVirtual     = ms->ullTotalPhys;
    ms->ullAvailVirtual     = ms->ullAvailPhys;
    ms->ullAvailExtendedVirtual = 0;
    return 1;
}

/* ---- psapi ----------------------------------------------------------------------------------- */
int __stdcall GetProcessMemoryInfo(void *proc, PPROCESS_MEMORY_COUNTERS c, unsigned long cb)
{
    (void)proc; (void)cb;
    if (!c) return 0;
    c->WorkingSetSize = 0;
    c->PagefileUsage = 0;
    return 1;
}

/* ---- MSVCRT thread starters over CreateThread ------------------------------------------------ */
unsigned long _beginthreadex(void *security, unsigned stack_size,
                             unsigned (__stdcall *start)(void *), void *arg,
                             unsigned initflag, unsigned *thrdaddr)
{
    HANDLE h = CreateThread((LPSECURITY_ATTRIBUTES)security, stack_size,
                            (LPTHREAD_START_ROUTINE)start, arg, initflag, (LPDWORD)thrdaddr);
    return (unsigned long)h;
}
void _endthreadex(unsigned retval) { ExitThread((DWORD)retval); }

/* ================================================================================================
 * Endgame link-surface: Win32 APIs the Windows-2000-era SDK omits that Mono references. Referenced
 * cdecl (undecorated) unless a @N stdcall decoration is noted. Simplified signatures are fine for
 * cdecl (caller cleans the stack); the @N ones must match arg-byte counts exactly.
 * ============================================================================================== */

/* ---- 64-bit / pointer Interlocked (over __sync; libxapi has 32-bit only) --------------------- */
long long InterlockedIncrement64(long long volatile *p)          { return __sync_add_and_fetch(p, 1); }
long long InterlockedDecrement64(long long volatile *p)          { return __sync_sub_and_fetch(p, 1); }
long long InterlockedAdd64(long long volatile *p, long long v)   { return __sync_add_and_fetch(p, v); }
long      InterlockedAdd(long volatile *p, long v)               { return __sync_add_and_fetch(p, v); }
long long InterlockedExchange64(long long volatile *p, long long v) { return __sync_lock_test_and_set(p, v); }
long long InterlockedCompareExchange64(long long volatile *p, long long ex, long long comp)
                                                                 { return __sync_val_compare_and_swap(p, comp, ex); }
void *InterlockedExchangePointer(void *volatile *p, void *v)     { return __sync_lock_test_and_set(p, v); }
void *InterlockedCompareExchangePointer(void *volatile *p, void *ex, void *comp)
                                                                 { return __sync_val_compare_and_swap(p, comp, ex); }

/* ---- wide (W) synchronization APIs -> the SDK's ANSI variants (wide names ignored) ------------ */
HANDLE CreateEventW(void *sa, int manual, int initial, const unsigned short *name)
{ (void)name; return CreateEventA((LPSECURITY_ATTRIBUTES)sa, manual, initial, NULL); }
HANDLE CreateMutexW(void *sa, int owner, const unsigned short *name)
{ (void)name; return CreateMutexA((LPSECURITY_ATTRIBUTES)sa, owner, NULL); }
HANDLE OpenEventW(unsigned long access, int inherit, const unsigned short *name)     { (void)access;(void)inherit;(void)name; return NULL; }
HANDLE OpenMutexW(unsigned long access, int inherit, const unsigned short *name)     { (void)access;(void)inherit;(void)name; return NULL; }
HANDLE OpenSemaphoreW(unsigned long access, int inherit, const unsigned short *name) { (void)access;(void)inherit;(void)name; return NULL; }
HANDLE OpenThread(unsigned long access, int inherit, unsigned long tid)              { (void)access;(void)inherit;(void)tid; return NULL; }

/* ---- SRW try-acquire + CS-Ex ----------------------------------------------------------------- */
int InitializeCriticalSectionEx(void *cs, unsigned long spin, unsigned long flags)
{ (void)spin;(void)flags; InitializeCriticalSection((CRITICAL_SECTION *)cs); return 1; }
int TryAcquireSRWLockExclusive(PSRWLOCK lock) { return TryEnterCriticalSection(srw_cs(lock)); }

/* ---- dynamic loading: unsupported on Xbox (static P/Invoke) ----------------------------------- */
void *LoadLibrary(const char *n)   { (void)n; return NULL; }
void *LoadLibraryW(const unsigned short *n) { (void)n; return NULL; }
void *GetProcAddress(void *m, const char *n) { (void)m;(void)n; return NULL; }
int   FreeLibrary(void *m)         { (void)m; return 1; }
void *__stdcall LoadLibraryExW(const unsigned short *n, void *h, unsigned long f) /* @12 */
{ (void)n;(void)h;(void)f; return NULL; }
void *__stdcall MonoLoadImage(const unsigned short *n) { (void)n; return NULL; } /* @4 */
void *coree_module_handle = NULL;   /* DATA symbol (coree.h extern) */

/* ---- environment: Xbox has none -------------------------------------------------------------- */
unsigned long GetEnvironmentVariableW(const unsigned short *n, unsigned short *buf, unsigned long sz)
{ (void)n;(void)buf;(void)sz; return 0; }
int  SetEnvironmentVariableW(const unsigned short *n, const unsigned short *v) { (void)n;(void)v; return 1; }
void *GetEnvironmentStrings(void) { return NULL; }
int  FreeEnvironmentStrings(void *p) { (void)p; return 1; }

/* ---- COM: disabled --------------------------------------------------------------------------- */
long CoInitializeEx(void *reserved, unsigned long model) { (void)reserved;(void)model; return 0; }
void CoUninitialize(void) {}

/* ---- vectored exception handlers: unused ----------------------------------------------------- */
void *AddVectoredExceptionHandler(unsigned long first, void *handler) { (void)first;(void)handler; return NULL; }
unsigned long RemoveVectoredExceptionHandler(void *h) { (void)h; return 0; }

/* ---- misc ------------------------------------------------------------------------------------ */
extern volatile unsigned long KeTickCount;   /* xboxkrnl export: ms-ish ticks since boot */
unsigned long long __stdcall GetTickCount64(void) { return (unsigned long long)KeTickCount; } /* @0 */
/* NOTE: GetTickCount + Sleep are declared __declspec(dllimport) by winbase.h, so they can't be
 * defined in a TU that includes <xtl.h>. Mono references them undecorated (cdecl); they're defined
 * in the headerless win_cdecl_shims.c instead. */
void FlushProcessWriteBuffers(void) {}
void GetCurrentProcessorNumberEx(void *procnum) { if (procnum) { unsigned short *p = (unsigned short*)procnum; p[0]=0; p[1]=0; } }
int  IsWow64Process(void *proc, int *result) { (void)proc; if (result) *result = 0; return 1; }
void *NtCurrentProcess(void) { return (void *)(unsigned long)-1; }
/* On desktop NT, NT_TIB.Self lives at fs:[0x18]. The original Xbox kernel leaves that slot 0 and
 * instead keeps the self-pointer of the fs-based control region at fs:[0x1C] (KPCR.SelfPcr), whose
 * NT_TIB (ExceptionList/StackBase/StackLimit at offsets 0/4/8) is the running thread's. Return that
 * so NT_TIB fields (stack bounds, exception list) are reachable. Verified on-device: fs[0x18]==0,
 * fs[0x1C] points to a region whose +4/+8 match the live StackBase/StackLimit. */
void *NtCurrentTeb(void) { void *teb; __asm__ __volatile__("movl %%fs:0x1c, %0" : "=r"(teb)); return teb; }
int  GetThreadContext(void *thread, void *ctx) { (void)thread;(void)ctx; return 0; }
int  WSAWaitForMultipleEvents(unsigned long n, const void *ev, int all, unsigned long ms, int alertable)
{ (void)n;(void)ev;(void)all;(void)ms;(void)alertable; return (int)0xFFFFFFFF; /* WSA_WAIT_FAILED */ }

int GetVersionExW(void *info)
{
    OSVERSIONINFOEXW *o = (OSVERSIONINFOEXW *)info;   /* first fields match OSVERSIONINFOW */
    if (!o) return 0;
    o->dwMajorVersion = 5; o->dwMinorVersion = 1; o->dwBuildNumber = 2600; o->dwPlatformId = 2;
    return 1;
}

/* ---- file mapping: read-into-buffer emulation --------------------------------------------------
 * The Xbox has NO Win32 file-mapping API (libxapi ships neither CreateFileMapping, MapViewOfFile
 * nor UnmapViewOfFile), and Mono's HOST_WIN32 image loader (metadata/image.c) maps assemblies via
 * mono_file_map -> CreateFileMappingW/MapViewOfFile with no fileio fallback compiled in. The
 * mono-filemap.c open path is worse still: it uses _wfopen with a UTF-16 path, and RXDK's wide file
 * APIs are broken on D:\ paths. So we override the whole mono_file_map_* family with loose defs
 * (they win the --allow-multiple-definition race over the archive copies) built ONLY on the ANSI
 * primitives verified to work on-device: CreateFileA / GetFileSize / SetFilePointer / ReadFile.
 * A "mapping" is just the whole (sub)range read into a malloc'd buffer — assemblies are small and
 * loaded read-only, so this is functionally identical to a private read-only mapping. MonoFileMap*
 * is opaque to Mono, so we stash the file HANDLE in it directly and hand the same HANDLE back as the
 * "fd" (32-bit target: HANDLE fits in int); mono_file_map then reads via that HANDLE. */
extern void *malloc(size_t);
extern void  free(void *);

void *mono_file_map_open(const char *name)
{
    HANDLE h = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return (h == INVALID_HANDLE_VALUE) ? NULL : (void *)h;
}
unsigned long long mono_file_map_size(void *fmap)
{
    DWORD hi = 0, lo = GetFileSize((HANDLE)fmap, &hi);
    return ((unsigned long long)hi << 32) | lo;
}
int mono_file_map_fd(void *fmap) { return (int)(size_t)fmap; }  /* HANDLE round-trips through int (32-bit) */
int mono_file_map_close(void *fmap) { return CloseHandle((HANDLE)fmap) ? 0 : -1; }

void *mono_file_map(size_t length, int flags, int fd, unsigned long long offset, void **ret_handle)
{
    HANDLE h = (HANDLE)(size_t)fd;
    void  *p;
    DWORD  got = 0;
    (void)flags;
    if (ret_handle) *ret_handle = NULL;
    p = malloc(length);
    if (!p)
        return NULL;
    SetFilePointer(h, (LONG)offset, NULL, FILE_BEGIN);
    if (!ReadFile(h, p, (DWORD)length, &got, NULL) || got != (DWORD)length) {
        free(p);
        return NULL;
    }
    if (ret_handle) *ret_handle = p;   /* handle == buffer; freed by mono_file_unmap */
    return p;
}
int mono_file_unmap(void *addr, void *handle) { (void)handle; free(addr); return 0; }

