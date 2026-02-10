/**
 * Syscalls stubs for newlib
 * Provides minimal implementations to silence linker warnings
 */

#include <sys/stat.h>
#include <errno.h>

#undef errno
extern int errno;

/* Stub implementations - these syscalls are not needed for bare-metal */

int _close(int file) {
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st) {
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _write(int file, char *ptr, int len) {
    (void)file;
    
    /* If you want to implement actual UART output, do it here */
    /* For now, just pretend we wrote everything */
    for (int i = 0; i < len; i++) {
        (void)ptr[i];  /* Suppress unused warning */
    }
    
    return len;
}
