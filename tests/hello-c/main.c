//-----------------------------------------------------------------------------
// RXDK-DotNet — C-XBE toolchain smoke test
//
// The smallest possible RXDK title: prints to the debug monitor and idles.
// Its only job is to prove the toolchain + engine + imagebld + xemu chain end
// to end BEFORE any managed-runtime work. No D3D, no framework.
//-----------------------------------------------------------------------------
#include <xtl.h>

void __cdecl main()
{
    OutputDebugStringA("RXDK-DotNet: C-XBE smoke test — hello from managed-runtime host\n");

    // Do not return from main(): a title that falls off the end would exit/reboot
    // immediately, giving xemu nothing to show. Idle instead.
    for (;;)
    {
        Sleep(1000);
    }
}
