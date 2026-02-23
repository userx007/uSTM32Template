#include "tx_api.h"

/* ThreadX memory pool — statically allocated */
#define DEMO_STACK_SIZE   1024
#define DEMO_BYTE_POOL_SIZE (9120 + DEMO_STACK_SIZE * 2)

static UCHAR byte_pool_memory[DEMO_BYTE_POOL_SIZE];

/* Forward declarations */
void tx_application_define(void *first_unused_memory);

int main(void)
{
    /* Optional: board-level hardware init (clocks, UART, etc.) BEFORE
       starting the kernel. Do NOT start SysTick here if ThreadX owns it. */
    //hardware_init();

    /* Enter the ThreadX kernel — never returns */
    tx_kernel_enter();

    return 0;   /* unreachable */
}