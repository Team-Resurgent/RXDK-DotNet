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
