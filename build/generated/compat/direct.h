/*
 * RXDK-DotNet — minimal <direct.h> shim for the Mono HOST_WIN32 build.
 * Mono's eglib gpath.c includes <direct.h> (MSVCRT) for the working-directory / dir CRT calls.
 * picolibc has no direct.h; declare the few names Mono uses, forwarded to POSIX in win_crt_compat.c
 * (or provided by picolibc where the POSIX name matches). Extend as needed.
 */
#ifndef RXDK_COMPAT_DIRECT_H
#define RXDK_COMPAT_DIRECT_H

#ifdef __cplusplus
extern "C" {
#endif

char *_getcwd(char *buffer, int maxlen);
int   _chdir(const char *path);
int   _mkdir(const char *path);
int   _rmdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* RXDK_COMPAT_DIRECT_H */
