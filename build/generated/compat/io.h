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

/* MSVCRT open() flag constants (gfile-win32.c uses _open with these). */
#ifndef _O_RDONLY
#define _O_RDONLY 0x0000
#define _O_WRONLY 0x0001
#define _O_RDWR   0x0002
#define _O_APPEND 0x0008
#define _O_CREAT  0x0100
#define _O_TRUNC  0x0200
#define _O_EXCL   0x0400
#define _O_TEXT   0x4000
#define _O_BINARY 0x8000
#endif
#ifndef _S_IREAD
#define _S_IREAD  0x0100
#define _S_IWRITE 0x0080
#endif

#ifdef __cplusplus
}
#endif

#endif /* RXDK_COMPAT_IO_H */
