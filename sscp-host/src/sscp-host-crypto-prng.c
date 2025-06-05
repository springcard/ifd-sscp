#include "sscp-host-crypto_i.h"

#ifdef _WIN32

#pragma comment(lib, "bcrypt.lib")

BOOL SSCP_GetRandom(BYTE buffer[], DWORD bufferSz)
{
    NTSTATUS status = BCryptGenRandom(
        NULL,
        (PUCHAR)buffer,
        (ULONG)bufferSz,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    return (status == 0); // STATUS_SUCCESS == 0
}

#else

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#include <fcntl.h>
#include <errno.h>

BOOL SSCP_GetRandom(BYTE buffer[], DWORD bufferSz)
{
    size_t offset = 0;

#if defined(__linux__)
    // Try getrandom() syscall
    while (offset < bufferSz)
    {
        ssize_t r = getrandom(buffer + offset, bufferSz - offset, 0);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        offset += r;
    }

    if (offset == bufferSz)
        return TRUE;
#endif

    // Fallback: read from /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return FALSE;

    offset = 0;
    while (offset < bufferSz)
    {
        ssize_t r = read(fd, buffer + offset, bufferSz - offset);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            return FALSE;
        }
        offset += r;
    }

    close(fd);
    return TRUE;
}

#endif
