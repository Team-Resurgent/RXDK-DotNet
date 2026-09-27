/*
 * RXDK-DotNet — minimal <objbase.h> stub for Mono's HOST_WIN32 build.
 * COM is disabled (DISABLE_COM); the COM interop files are excluded. A couple of headers still
 * #include <objbase.h> transitively, so this satisfies the include. COM types come from the SDK's
 * windows headers where actually referenced; extend only if a compiled file needs a specific symbol.
 */
#ifndef RXDK_COMPAT_OBJBASE_H
#define RXDK_COMPAT_OBJBASE_H
#endif
