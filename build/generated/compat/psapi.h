/*
 * RXDK-DotNet — minimal <psapi.h> shim for Mono's HOST_WIN32 build.
 * Process-memory query API; Mono uses it for counters/stats. Xbox has no psapi; the one Mono file
 * that includes it is a stats path. Declare the struct + function; shim (or no-op) in
 * win32_supplement.c. Extend as needed.
 */
#ifndef RXDK_COMPAT_PSAPI_H
#define RXDK_COMPAT_PSAPI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PROCESS_MEMORY_COUNTERS {
    unsigned long  cb;
    unsigned long  PageFaultCount;
    unsigned long  PeakWorkingSetSize;
    unsigned long  WorkingSetSize;
    unsigned long  QuotaPeakPagedPoolUsage;
    unsigned long  QuotaPagedPoolUsage;
    unsigned long  QuotaPeakNonPagedPoolUsage;
    unsigned long  QuotaNonPagedPoolUsage;
    unsigned long  PagefileUsage;
    unsigned long  PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS, *PPROCESS_MEMORY_COUNTERS;

int __stdcall GetProcessMemoryInfo(void *Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, unsigned long cb);

#ifdef __cplusplus
}
#endif

#endif /* RXDK_COMPAT_PSAPI_H */
