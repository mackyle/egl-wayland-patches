#define _POSIX_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#define EGLBoolean int
#define EGL_FALSE 0

EGLBoolean wlEglMemoryIsReadable(const void *p, size_t len);


static int try_pipe_write(int fd, const void *p, size_t len)
{
    int result;

    do {
        result = write(fd, p, len);
    } while (result == -1 && errno == EINTR);
    if (result == -1 && errno == EAGAIN) {
        result = 0;
    }
    if (result == -1 && errno != EFAULT && errno != EINVAL)
        fprintf(stderr, "result=-1 errno=%d\n", errno);
    assert(result != -1 || errno == EFAULT || errno == EINVAL);
    return result;
}

EGLBoolean wlEglMemoryIsReadable(const void *p, size_t len)
{
    int fds[2], result = -1;
    if (pipe(fds) == -1) {
        return EGL_FALSE;
    }
    if (len == 0) {
        len = 1;
    }

    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) == -1) {
        goto done;
    }
    if (fcntl(fds[1], F_SETFL, O_NONBLOCK) == -1) {
        goto done;
    }

    /* write will fail with EFAULT if the provided buffer is outside
     * our accessible address space. */
    result = try_pipe_write(fds[1], (const char *)p + (len - 1), 1);
    if (result != -1) {
        char drain;
        /* restore full PIPE_BUF space in pipe */
        while (read(fds[0], &drain, 1) == -1 && errno == EINTR) {
            /* retry */
        }
        result = try_pipe_write(fds[1], p, len);
    }

done:
    close(fds[0]);
    close(fds[1]);
    return result != -1;
}


int main(int argc, char **argv)
{
	char buf[32768];
	void *block = malloc(32768);
	char *zp = NULL;
	size_t l = 32768U;
	size_t stmp1 = (size_t)SSIZE_MAX + 1;
	EGLBoolean z0, r0, r1;

	(void)argc; (void)argv;
	
	printf("SSIZE_MAX is 0x%llx\n", (unsigned long long)SSIZE_MAX);
	printf("buf readable for SSIZE_MAX+1: %d\n\n", wlEglMemoryIsReadable(
		(void *)((uintptr_t)(void *)buf-(stmp1-1)), stmp1));

	do {
		unsigned k = l / 1024U;
		z0 = wlEglMemoryIsReadable(zp, 32768);
		r0 = wlEglMemoryIsReadable(block, l);
		r1 = wlEglMemoryIsReadable(buf, l);
		printf(" zero readable for %uK: %d (at %p)\n", 32, (int)z0, zp);
		printf("block readable for %uK: %d (at %p)\n", k,  (int)r0, block);
		printf("  buf readable for %uK: %d (at %p)\n", k,  (int)r1, &buf[0]);
		l += 32768U;
		zp += 32768U;
	} while (r0 || r1);

	return 0;
}
