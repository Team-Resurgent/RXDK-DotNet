/*
 * RXDK-DotNet — MSVCRT file-IO name compat (see win_crt_compat.h).
 * Thin forwarders from the MSVCRT underscore names Mono's HOST_WIN32 code calls to the POSIX
 * functions RXDK's picolibc libc provides.
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "rxdk/win_crt_compat.h"

/* _ecvt_s: convert `value` to `count` significant decimal digits (no sign, no point) in `buffer`,
 * with *dec = position of the decimal point relative to the start of the digits, *sign = negative.
 * corefx's Number.Windows.cs DoubleToNumber relies on this for ALL Double/Single.ToString — without
 * a working one every float formats as "0". We implement it via snprintf("%.*e") so we don't depend
 * on RXDK's CRT _ecvt_s (a no-op on-device). NaN/Inf are handled by the caller before we're reached. */
int _ecvt_s(char *buffer, size_t sizeInBytes, double value, int count, int *dec, int *sign)
{
    char tmp[512];
    char digits[64];
    int i = 0, nd = 0, e = 0, es = 1, di;

    if (!buffer || sizeInBytes == 0) return 22;   /* EINVAL */
    if (count < 1) count = 1;
    if (count > 40) count = 40;
    if (sign) *sign = 0;
    if (value < 0) { if (sign) *sign = 1; value = -value; }

    /* count significant digits: one before the point, count-1 after, plus exponent. */
    snprintf(tmp, sizeof(tmp), "%.*e", count - 1, value);

    if (tmp[i] >= '0' && tmp[i] <= '9') digits[nd++] = tmp[i++];
    if (tmp[i] == '.') { i++; while (tmp[i] >= '0' && tmp[i] <= '9' && nd < (int)sizeof(digits)) digits[nd++] = tmp[i++]; }
    if (tmp[i] == 'e' || tmp[i] == 'E') {
        i++;
        if (tmp[i] == '+') i++; else if (tmp[i] == '-') { es = -1; i++; }
        while (tmp[i] >= '0' && tmp[i] <= '9') { e = e * 10 + (tmp[i] - '0'); i++; }
        e *= es;
    }
    if (dec) *dec = e + 1;   /* the point sits just after the first significant digit (10^e) */

    for (di = 0; di < nd && (size_t)di < sizeInBytes - 1; di++) buffer[di] = digits[di];
    buffer[di] = '\0';
    return 0;
}

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

/* NOTE: _ecvt_s is implemented for real at the top of this file — it is NOT "unused": corefx's
 * Number.Windows.cs DoubleToNumber calls it for every Double/Single.ToString. The old no-op stub
 * here made all floating-point formatting produce "0". */

/* Write a dword through the FS segment (SEH/TEB slot writes). */
void __writefsdword(unsigned long offset, unsigned long value)
{
    __asm__ __volatile__("movl %0, %%fs:(%1)" : : "r"(value), "r"(offset) : "memory");
}
