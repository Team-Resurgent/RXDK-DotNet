/*
 * RXDK-DotNet — Win32 supplement for Mono's HOST_WIN32 build on the original Xbox.
 *
 * Mono 6.13's HOST_WIN32 path assumes a modern (Vista+) Win32 surface, but the Xbox kernel + RXDK
 * SDK expose a Windows 2000-era surface. This header supplies the types/constants Mono references
 * that the SDK's Windows headers lack. It is FORCE-INCLUDED (clang -include) ahead of the SDK
 * headers so the definitions are visible in every TU (some are reached via atomic.h / mono-os-mutex.h
 * before any windows header). Types are self-contained; the Vista-only primitives (SRW locks) and the
 * *Ex query functions are emulated in win32_supplement.c over what RXDK/libxapi/xboxkrnl provide
 * (CRITICAL_SECTION, MmQueryStatistics, __sync intrinsics).
 *
 * These are genuine Win32 declarations the SDK should eventually carry — TODO: upstream to RXDK-SDK.
 */
#ifndef RXDK_WIN32_SUPPLEMENT_H
#define RXDK_WIN32_SUPPLEMENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* 64-bit integer aliases used by atomic.h's Interlocked64 wrappers. */
#ifndef _LONG64_RXDK_DEFINED
#define _LONG64_RXDK_DEFINED
typedef long long          LONG64;
typedef unsigned long long ULONG64;
#endif

/* Fixed-width Win32 int aliases the SDK headers don't define. */
#ifndef RXDK_UINT64_DEFINED
#define RXDK_UINT64_DEFINED
typedef unsigned long long UINT64;
typedef long long          INT64;
#endif

/* Per-process TLS slot count (Win32 guarantees at least 64). */
#ifndef TLS_MINIMUM_AVAILABLE
#define TLS_MINIMUM_AVAILABLE 64
#endif

/* Flag for InitializeCriticalSectionEx (Vista+); harmless on the plain InitializeCriticalSection
 * path RXDK provides. */
#ifndef CRITICAL_SECTION_NO_DEBUG_INFO
#define CRITICAL_SECTION_NO_DEBUG_INFO 0x01000000
#endif

/* Assorted Win32 constants the SDK headers omit. */
#ifndef DUPLICATE_SAME_ACCESS
#define DUPLICATE_SAME_ACCESS 0x00000002
#endif
#ifndef MAXIMUM_WAIT_OBJECTS
#define MAXIMUM_WAIT_OBJECTS 64
#endif
#ifndef HEAP_CREATE_ENABLE_EXECUTE
#define HEAP_CREATE_ENABLE_EXECUTE 0x00040000
#endif
#ifndef MEMORY_ALLOCATION_ALIGNMENT
#define MEMORY_ALLOCATION_ALIGNMENT 8
#endif

/* Thread Information Block — Mono reads StackBase/StackLimit from fs:[0] for stack bounds. */
#ifndef RXDK_NT_TIB_DEFINED
#define RXDK_NT_TIB_DEFINED
typedef struct _NT_TIB {
    void *ExceptionList;
    void *StackBase;
    void *StackLimit;
    void *SubSystemTib;
    union { void *FiberData; unsigned long Version; };
    void *ArbitraryUserPointer;
    struct _NT_TIB *Self;
} NT_TIB, *PNT_TIB;
#endif

/* PROCESSOR_NUMBER (Win7+ affinity); Xbox is single-core, only appears in a signature. */
#ifndef RXDK_PROCESSOR_NUMBER_DEFINED
#define RXDK_PROCESSOR_NUMBER_DEFINED
typedef struct _PROCESSOR_NUMBER { unsigned short Group; unsigned char Number; unsigned char Reserved; } PROCESSOR_NUMBER, *PPROCESSOR_NUMBER;
#endif

/* Winsock event handle — sockets are disabled; only appears in signatures. */
#ifndef RXDK_WSAEVENT_DEFINED
#define RXDK_WSAEVENT_DEFINED
typedef void *WSAEVENT;
#endif

/* Slim Reader/Writer lock — Vista+; absent on Win2000/Xbox. Emulated over a CRITICAL_SECTION
 * in win32_supplement.c (exclusive == shared for correctness; contention is fine for bring-up). */
#ifndef RXDK_SRWLOCK_DEFINED
#define RXDK_SRWLOCK_DEFINED
typedef struct _RXDK_SRWLOCK { void *Ptr; } SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT { 0 }
void __stdcall InitializeSRWLock(PSRWLOCK);
void __stdcall AcquireSRWLockExclusive(PSRWLOCK);
void __stdcall ReleaseSRWLockExclusive(PSRWLOCK);
void __stdcall AcquireSRWLockShared(PSRWLOCK);
void __stdcall ReleaseSRWLockShared(PSRWLOCK);
#endif

/* Condition variables — Vista+; absent on Win2000/Xbox. Emulated over event/CRITICAL_SECTION in
 * win32_supplement.c. The CS parameter is void* to avoid depending on CRITICAL_SECTION's definition
 * order (it comes from the SDK headers, included after this force-include). */
#ifndef RXDK_CONDITION_VARIABLE_DEFINED
#define RXDK_CONDITION_VARIABLE_DEFINED
typedef struct _RXDK_CONDVAR { void *Ptr; } CONDITION_VARIABLE, *PCONDITION_VARIABLE;
#define CONDITION_VARIABLE_INIT { 0 }
void __stdcall InitializeConditionVariable(PCONDITION_VARIABLE);
int  __stdcall SleepConditionVariableCS(PCONDITION_VARIABLE, void *lpCriticalSection, unsigned long dwMilliseconds);
int  __stdcall SleepConditionVariableSRW(PCONDITION_VARIABLE, PSRWLOCK, unsigned long dwMilliseconds, unsigned long Flags);
void __stdcall WakeConditionVariable(PCONDITION_VARIABLE);
void __stdcall WakeAllConditionVariable(PCONDITION_VARIABLE);
#endif

/* GetSystemInfo / SYSTEM_INFO. */
#ifndef RXDK_SYSTEM_INFO_DEFINED
#define RXDK_SYSTEM_INFO_DEFINED
typedef struct _SYSTEM_INFO {
    union {
        unsigned long dwOemId;
        struct { unsigned short wProcessorArchitecture; unsigned short wReserved; };
    };
    unsigned long  dwPageSize;
    void          *lpMinimumApplicationAddress;
    void          *lpMaximumApplicationAddress;
    unsigned long  dwActiveProcessorMask;    /* ULONG_PTR (32-bit on i686) */
    unsigned long  dwNumberOfProcessors;
    unsigned long  dwProcessorType;
    unsigned long  dwAllocationGranularity;
    unsigned short wProcessorLevel;
    unsigned short wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;
void __stdcall GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
#endif

/* GlobalMemoryStatusEx / MEMORYSTATUSEX (the SDK has the older MEMORYSTATUS only). */
#ifndef RXDK_MEMORYSTATUSEX_DEFINED
#define RXDK_MEMORYSTATUSEX_DEFINED
typedef struct _MEMORYSTATUSEX {
    unsigned long      dwLength;
    unsigned long      dwMemoryLoad;
    unsigned long long ullTotalPhys;
    unsigned long long ullAvailPhys;
    unsigned long long ullTotalPageFile;
    unsigned long long ullAvailPageFile;
    unsigned long long ullTotalVirtual;
    unsigned long long ullAvailVirtual;
    unsigned long long ullAvailExtendedVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;
int __stdcall GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RXDK_WIN32_SUPPLEMENT_H */
