/*
 * RXDK-DotNet — minimal <winsock2.h> stub for Mono's HOST_WIN32 build on Xbox.
 * Sockets are DISABLED; Mono includes <winsock2.h> defensively from core headers, so it must exist.
 * The Win32 types/constants Mono actually needs live in the force-included win32_supplement.h.
 */
#ifndef RXDK_COMPAT_WINSOCK2_H
#define RXDK_COMPAT_WINSOCK2_H
#include <rxdk/win32_supplement.h>
#include <sys/_timeval.h>   /* struct timeval — Mono includes winsock2.h expecting it (threads.c) */
#endif
