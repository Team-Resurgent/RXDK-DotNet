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

#include <time.h>   /* time_t for the MSVCRT time structs below */

#ifdef __cplusplus
extern "C" {
#endif

/* MSVCRT time structs the SDK forward-declares but doesn't define. */
#ifndef RXDK__TIMEB_DEFINED
#define RXDK__TIMEB_DEFINED
struct _timeb { time_t time; unsigned short millitm; short timezone; short dstflag; };
#endif
#ifndef RXDK_UTIMBUF_DEFINED
#define RXDK_UTIMBUF_DEFINED
struct utimbuf { time_t actime; time_t modtime; };
#endif

/* OSVERSIONINFO (non-EX), ANSI + generic alias (GetVersionEx). */
#ifndef RXDK_OSVERSIONINFO_DEFINED
#define RXDK_OSVERSIONINFO_DEFINED
typedef struct _OSVERSIONINFOA {
    unsigned long dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
    char          szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;
#ifndef OSVERSIONINFO
#define OSVERSIONINFO OSVERSIONINFOA
#endif
#endif

/* ReplaceFile flag (w32file). */
#ifndef REPLACEFILE_IGNORE_MERGE_ERRORS
#define REPLACEFILE_IGNORE_MERGE_ERRORS 0x00000002
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

/* COM apartment-init flags — COM is disabled, but threads.c references these unconditionally. */
#ifndef COINIT_APARTMENTTHREADED
#define COINIT_MULTITHREADED      0x0
#define COINIT_APARTMENTTHREADED  0x2
#define COINIT_DISABLE_OLE1DDE    0x4
#define COINIT_SPEED_OVER_MEMORY  0x8
#endif

/* Winsock types — sockets are disabled; these only appear in icall-table.h signatures. */
#ifndef RXDK_WSAEVENT_DEFINED
#define RXDK_WSAEVENT_DEFINED
typedef void *WSAEVENT;
#endif
#ifndef RXDK_WSABUF_DEFINED
#define RXDK_WSABUF_DEFINED
typedef struct _WSABUF { unsigned long len; char *buf; } WSABUF, *LPWSABUF;
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

/* File attribute / file type constants the SDK omits (w32file, console). */
#ifndef FILE_ATTRIBUTE_ENCRYPTED
#define FILE_ATTRIBUTE_ENCRYPTED       0x00004000
#endif
#ifndef FILE_ATTRIBUTE_REPARSE_POINT
#define FILE_ATTRIBUTE_REPARSE_POINT   0x00000400
#endif
#ifndef FILE_ATTRIBUTE_SPARSE_FILE
#define FILE_ATTRIBUTE_SPARSE_FILE     0x00000200
#endif
#ifndef FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#endif
#ifndef FILE_TYPE_CHAR
#define FILE_TYPE_UNKNOWN 0x0000
#define FILE_TYPE_DISK    0x0001
#define FILE_TYPE_CHAR    0x0002
#define FILE_TYPE_PIPE    0x0003
#endif

/* WIN32_FIND_DATA (FindFirstFile) — self-contained (ft* as low/high DWORD pairs to avoid depending
 * on FILETIME's definition order). */
#ifndef RXDK_WIN32_FIND_DATA_DEFINED
#define RXDK_WIN32_FIND_DATA_DEFINED
typedef struct _RXDK_FILETIME { unsigned long dwLowDateTime; unsigned long dwHighDateTime; } RXDK_FILETIME;
typedef struct _WIN32_FIND_DATAW {
    unsigned long  dwFileAttributes;
    RXDK_FILETIME  ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    unsigned long  nFileSizeHigh, nFileSizeLow;
    unsigned long  dwReserved0, dwReserved1;
    unsigned short cFileName[260];
    unsigned short cAlternateFileName[14];
} WIN32_FIND_DATAW, *LPWIN32_FIND_DATAW, *PWIN32_FIND_DATAW;
/* NOTE: the SDK's winbase.h already defines the ANSI WIN32_FIND_DATAA; only the wide W variant is
 * missing, so we define only that (struct tags can't be #ifndef-guarded against the SDK). */
#endif

/* OSVERSIONINFOEX (GetVersionEx), wide + ANSI. */
#ifndef RXDK_OSVERSIONINFOEX_DEFINED
#define RXDK_OSVERSIONINFOEX_DEFINED
typedef struct _OSVERSIONINFOEXW {
    unsigned long  dwOSVersionInfoSize;
    unsigned long  dwMajorVersion;
    unsigned long  dwMinorVersion;
    unsigned long  dwBuildNumber;
    unsigned long  dwPlatformId;
    unsigned short szCSDVersion[128];
    unsigned short wServicePackMajor;
    unsigned short wServicePackMinor;
    unsigned short wSuiteMask;
    unsigned char  wProductType;
    unsigned char  wReserved;
} OSVERSIONINFOEXW, *LPOSVERSIONINFOEXW, *POSVERSIONINFOEXW;
typedef struct _OSVERSIONINFOEXA {
    unsigned long  dwOSVersionInfoSize;
    unsigned long  dwMajorVersion;
    unsigned long  dwMinorVersion;
    unsigned long  dwBuildNumber;
    unsigned long  dwPlatformId;
    char           szCSDVersion[128];
    unsigned short wServicePackMajor;
    unsigned short wServicePackMinor;
    unsigned short wSuiteMask;
    unsigned char  wProductType;
    unsigned char  wReserved;
} OSVERSIONINFOEXA, *LPOSVERSIONINFOEXA, *POSVERSIONINFOEXA;
#ifndef OSVERSIONINFOEX
#define OSVERSIONINFOEX OSVERSIONINFOEXA
#endif
#endif

/* PE image DOS header — coree.h (pulled in via image.h) references IMAGE_DOS_HEADER; the SDK's
 * windows.h path doesn't expose it. coree itself is excluded (Windows PE-EE hosting, unused). */
#ifndef RXDK_IMAGE_DOS_HEADER_DEFINED
#define RXDK_IMAGE_DOS_HEADER_DEFINED
typedef struct _IMAGE_DOS_HEADER {
    unsigned short e_magic, e_cblp, e_cp, e_crlc, e_cparhdr, e_minalloc, e_maxalloc;
    unsigned short e_ss, e_sp, e_csum, e_ip, e_cs, e_lfarlc, e_ovno, e_res[4];
    unsigned short e_oemid, e_oeminfo, e_res2[10];
    long           e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;
#endif

/* ReplaceFile flag (w32file). */
#ifndef REPLACEFILE_WRITE_THROUGH
#define REPLACEFILE_WRITE_THROUGH 0x00000001
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
