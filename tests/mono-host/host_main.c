/*
 * RXDK-DotNet — minimal Mono embedding host (Phase-1 endgame).
 * References mono_jit_init so the linker pulls the runtime-init closure; used first to enumerate
 * the remaining undefined-symbol surface, then to actually boot the interpreter.
 */
#include <xtl.h>

typedef struct _MonoDomain MonoDomain;
extern MonoDomain *mono_jit_init(const char *file);
extern void mono_jit_cleanup(MonoDomain *domain);

void __cdecl main(void)
{
    MonoDomain *domain;
    OutputDebugStringA("RXDK-DotNet: mono embedding host starting\n");
    domain = mono_jit_init("rxdk-dotnet");
    OutputDebugStringA(domain ? "mono_jit_init: OK\n" : "mono_jit_init: returned NULL\n");
    for (;;)
        Sleep(1000);
}
