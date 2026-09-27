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
