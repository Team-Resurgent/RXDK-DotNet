/*
 * RXDK-DotNet — minimal <io.h> shim for the Mono HOST_WIN32 build.
 * Mono's win32 eglib backends #include <io.h> but mostly use real Win32 APIs; the only MSVCRT
 * low-IO name they touch is _open (declared in win_crt_compat.h). This provides the header so the
 * includes resolve, plus a couple of commonly-referenced extras. Extend as needed.
 */
#ifndef RXDK_COMPAT_IO_H
#define RXDK_COMPAT_IO_H

#include "rxdk/win_crt_compat.h"   /* _open/_read/_write/_close/_lseek */

#ifdef __cplusplus
extern "C" {
#endif

int _access(const char *path, int mode);

#ifdef __cplusplus
}
#endif

#endif /* RXDK_COMPAT_IO_H */
