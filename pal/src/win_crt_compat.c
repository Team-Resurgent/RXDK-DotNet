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
