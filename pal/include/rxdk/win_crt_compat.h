/*
 * RXDK-DotNet — MSVCRT file-IO name compat for the Mono HOST_WIN32 build.
 *
 * Mono's glib.h (and w32file) under G_OS_WIN32 use the MSVCRT underscore CRT names (_read/_write/
 * _open/_close/_lseek/_unlink/_mktemp). RXDK's picolibc-based libc exposes the POSIX names
 * (read/write/…) and already provides the string/printf underscore variants (_snprintf/_stricmp/…)
 * but NOT these file-IO ones. This header DECLARES them (so TUs compile) and win_crt_compat.c
 * DEFINES them as thin forwarders to POSIX (so the runtime links).
 *
 * Force-included (clang -include) ahead of every Mono TU. Kept minimal; extend as more MSVCRT
 * names surface during the Phase-1 build. TODO: upstream into RXDK-Libs ms_crt_compat.c.
 */
#ifndef RXDK_WIN_CRT_COMPAT_H
#define RXDK_WIN_CRT_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

int   _read(int fd, void *buffer, unsigned int count);
int   _write(int fd, const void *buffer, unsigned int count);
int   _open(const char *path, int oflag, ...);
int   _close(int fd);
long  _lseek(int fd, long offset, int origin);
int   _unlink(const char *path);
char *_mktemp(char *template_);

#ifdef __cplusplus
}
#endif

#endif /* RXDK_WIN_CRT_COMPAT_H */
