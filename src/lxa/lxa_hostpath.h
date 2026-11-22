#ifndef LXA_HOSTPATH_H
#define LXA_HOSTPATH_H

/*
 * Portable replacements for /proc/self/exe and /proc/self/fd/N.
 * Both return the path length (>0) on success and -1 on failure.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef __APPLE__
#include <fcntl.h>
#include <stdint.h>
#include <mach-o/dyld.h>
#endif

static inline long lxa_exe_path(char *buf, size_t bufsize)
{
#ifdef __APPLE__
    char tmp[PATH_MAX], real[PATH_MAX];
    uint32_t sz = sizeof(tmp);
    if (_NSGetExecutablePath(tmp, &sz) != 0 || !realpath(tmp, real))
        return -1;
    size_t n = strlen(real);
    if (n >= bufsize)
        return -1;
    memcpy(buf, real, n + 1);
    return (long)n;
#else
    ssize_t len = readlink("/proc/self/exe", buf, bufsize - 1);
    if (len <= 0)
        return -1;
    buf[len] = '\0';
    return (long)len;
#endif
}

static inline long lxa_fd_path(int fd, char *buf, size_t bufsize)
{
#ifdef __APPLE__
    char tmp[PATH_MAX];
    if (fcntl(fd, F_GETPATH, tmp) == -1)
        return -1;
    size_t n = strlen(tmp);
    if (n >= bufsize)
        return -1;
    memcpy(buf, tmp, n + 1);
    return (long)n;
#else
    char link[64];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t len = readlink(link, buf, bufsize - 1);
    if (len < 0 || (size_t)len == bufsize - 1)
        return -1;      /* error, or possibly truncated */
    buf[len] = '\0';
    return (long)len;
#endif
}

#endif /* LXA_HOSTPATH_H */
