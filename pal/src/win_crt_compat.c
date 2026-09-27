/*
 * RXDK-DotNet — MSVCRT file-IO name compat (see win_crt_compat.h).
 * Thin forwarders from the MSVCRT underscore names Mono's HOST_WIN32 code calls to the POSIX
 * functions RXDK's picolibc libc provides.
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "rxdk/win_crt_compat.h"

int _read(int fd, void *buffer, unsigned int count)
{
    return (int)read(fd, buffer, (size_t)count);
}

int _write(int fd, const void *buffer, unsigned int count)
{
    return (int)write(fd, buffer, (size_t)count);
}

int _open(const char *path, int oflag, ...)
{
    /* Mode bits for O_CREAT are not used by the Mono paths that reach here; pass 0666. */
    return open(path, oflag, 0666);
}

int _close(int fd)
{
    return close(fd);
}

long _lseek(int fd, long offset, int origin)
{
    return (long)lseek(fd, (off_t)offset, origin);
}

int _unlink(const char *path)
{
    return unlink(path);
}

char *_mktemp(char *template_)
{
    /* picolibc doesn't provide mktemp; bring-up stub returns the template unchanged. Revisit if a
     * Mono path actually depends on a unique temp name. */
    return template_;
}

/* ---- endgame link-surface: MSVCRT names Mono uses that picolibc lacks ------------------------ */
#include <time.h>
#include "rxdk/win32_supplement.h"   /* struct _timeb */

int _access(const char *path, int mode) { return access(path, mode); }
int _fileno(FILE *stream)               { return fileno(stream); }

void _ftime(struct _timeb *tb)
{
    if (!tb) return;
    tb->time = time(NULL); tb->millitm = 0; tb->timezone = 0; tb->dstflag = 0;
}

int  _wmkdir(const unsigned short *path) { (void)path; return -1; }   /* no wide mkdir on Xbox */
int  utime(const char *path, const void *times) { (void)path; (void)times; return 0; }

int _ecvt_s(char *buf, unsigned long sz, double value, int ndigits, int *dec, int *sign)
{
    (void)buf; (void)sz; (void)value; (void)ndigits;
    if (dec) *dec = 0;
    if (sign) *sign = 0;
    return 0;   /* unused float-formatting path */
}

/* Write a dword through the FS segment (SEH/TEB slot writes). */
void __writefsdword(unsigned long offset, unsigned long value)
{
    __asm__ __volatile__("movl %0, %%fs:(%1)" : : "r"(value), "r"(offset) : "memory");
}
