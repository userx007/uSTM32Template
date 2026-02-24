/* syscalls.c – minimal newlib syscall stubs for bare-metal */
#include <sys/stat.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x) (void)x

int _close(int fd)                        
{ 
    UNUSED(fd);
    
    return -1; 
}

int _fstat(int fd, struct stat *st)       
{ 
    UNUSED(fd);
    UNUSED(st);

    st->st_mode = S_IFCHR; 

    return 0; 
}

int _isatty(int fd)                       
{ 
    UNUSED(fd);

    return 1; 
}

int _lseek(int fd, int ptr, int dir)      
{ 
    UNUSED(fd);
    UNUSED(ptr);
    UNUSED(dir);

    return 0; 
}

int _read(int fd, char *ptr, int len)     
{ 
    UNUSED(fd);
    UNUSED(ptr);
    UNUSED(len);

    return 0; 
}

int _getpid(void)                         
{ 
    return 1; 
}

int _kill(int pid, int sig)               
{ 
    UNUSED(pid);
    UNUSED(sig);

    errno = EINVAL; 

    return -1; 
}

void _init(void) 
{

}

void _fini(void) 
{

}

/* redirect to UART or semihosting for printf */
int _write(int fd, char *ptr, int len)
{
    UNUSED(fd);
    UNUSED(ptr);
    UNUSED(len);
/*    
    for (int i = 0; i < len; i++)
    {
        //uart_putchar(ptr[i]);  
    } 
*/    
    return len;
}

/* _sbrk: heap management – required if you use malloc/new */
extern char _end;       /* defined by linker script */
void *_sbrk(int incr) 
{
    static char *heap = &_end;
    char *prev = heap;
    heap += incr;
    return (void *)prev;
}

#ifdef __cplusplus
}
#endif