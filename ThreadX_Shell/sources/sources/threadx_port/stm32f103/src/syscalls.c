/* syscalls.c – minimal newlib syscall stubs for bare-metal */
#include <sys/stat.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

int _close(int fd)                        { return -1; }
int _fstat(int fd, struct stat *st)       { st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                       { return 1; }
int _lseek(int fd, int ptr, int dir)      { return 0; }
int _read(int fd, char *ptr, int len)     { return 0; }
int _getpid(void)                         { return 1; }
int _kill(int pid, int sig)               { errno = EINVAL; return -1; }
void _init(void) {}
void _fini(void) {}

/* _write: redirect to UART or semihosting here if you want printf */
// syscalls.c
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++); //uart_putchar(ptr[i]);
    return len;
}

/* _sbrk: heap management – required if you use malloc/new */
extern char _end;       /* defined by linker script */
void *_sbrk(int incr) {
    static char *heap = &_end;
    char *prev = heap;
    heap += incr;
    return (void *)prev;
}

#ifdef __cplusplus
}
#endif