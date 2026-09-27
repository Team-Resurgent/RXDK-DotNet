/*
 * RXDK-DotNet — minimal <process.h> shim for Mono's HOST_WIN32 build.
 * Mono's mono-threads-windows.c uses the MSVCRT thread starters _beginthreadex/_endthreadex from
 * <process.h>. RXDK creates threads via CreateThread/PsCreateSystemThreadEx; these are declared
 * here and shimmed (or routed to CreateThread) in win32_supplement.c. Extend as needed.
 */
#ifndef RXDK_COMPAT_PROCESS_H
#define RXDK_COMPAT_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned long _beginthreadex(void *security, unsigned stack_size,
                             unsigned (__stdcall *start_address)(void *),
                             void *arglist, unsigned initflag, unsigned *thrdaddr);
void _endthreadex(unsigned retval);

#ifdef __cplusplus
}
#endif

#endif /* RXDK_COMPAT_PROCESS_H */
