/**
 * @file    startup_stm32f4xx.s
 * @brief   STM32F4xx startup file for GCC (ARM Cortex-M4)
 *          Compatible with ThreadX RTOS
 *
 * This file:
 *  - Defines the interrupt vector table
 *  - Sets up the initial stack pointer
 *  - Calls SystemInit() then main()
 *  - Provides weak default handlers for all exceptions/interrupts
 */

  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

/*===========================================================================
 * 1.  Stack and Heap sizes  (override via linker or -D flags)
 *=========================================================================*/
  .equ  Stack_Size, 0x00000400   /* 1 KB – increase as needed for ThreadX  */
  .section .stack, "w", %nobits
  .align 3
__StackLimit:
  .space Stack_Size
  .globl __StackTop
__StackTop:

  .equ  Heap_Size, 0x00000200    /* 512 B – increase for dynamic allocation */
  .section .heap, "w", %nobits
  .align 3
  .globl __HeapBase
__HeapBase:
  .space Heap_Size
  .globl __HeapLimit
__HeapLimit:

/*===========================================================================
 * 2.  Vector Table
 *=========================================================================*/
  .section .isr_vector, "a", %progbits
  .global g_pfnVectors
  .type   g_pfnVectors, %object
  .size   g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  /* ARM Cortex-M4 core exceptions */
  .word  __StackTop                   /*  0: Initial Stack Pointer          */
  .word  Reset_Handler                /*  1: Reset                          */
  .word  NMI_Handler                  /*  2: Non-Maskable Interrupt         */
  .word  HardFault_Handler            /*  3: Hard Fault                     */
  .word  MemManage_Handler            /*  4: MPU Fault                      */
  .word  BusFault_Handler             /*  5: Bus Fault                      */
  .word  UsageFault_Handler           /*  6: Usage Fault                    */
  .word  0                            /*  7: Reserved                       */
  .word  0                            /*  8: Reserved                       */
  .word  0                            /*  9: Reserved                       */
  .word  0                            /* 10: Reserved                       */
  .word  SVC_Handler                  /* 11: SVCall  (ThreadX uses this)    */
  .word  DebugMon_Handler             /* 12: Debug Monitor                  */
  .word  0                            /* 13: Reserved                       */
  .word  PendSV_Handler               /* 14: PendSV  (ThreadX uses this)    */
  .word  SysTick_Handler              /* 15: SysTick (ThreadX uses this)    */

  /* STM32F4xx external interrupts (IRQ0 – IRQ85) */
  .word  WWDG_IRQHandler              /* 16: Window WatchDog                */
  .word  PVD_IRQHandler               /* 17: PVD through EXTI Line          */
  .word  TAMP_STAMP_IRQHandler        /* 18: Tamper and TimeStamps          */
  .word  RTC_WKUP_IRQHandler          /* 19: RTC Wakeup                     */
  .word  FLASH_IRQHandler             /* 20: FLASH                          */
  .word  RCC_IRQHandler               /* 21: RCC                            */
  .word  EXTI0_IRQHandler             /* 22: EXTI Line0                     */
  .word  EXTI1_IRQHandler             /* 23: EXTI Line1                     */
  .word  EXTI2_IRQHandler             /* 24: EXTI Line2                     */
  .word  EXTI3_IRQHandler             /* 25: EXTI Line3                     */
  .word  EXTI4_IRQHandler             /* 26: EXTI Line4                     */
  .word  DMA1_Stream0_IRQHandler      /* 27: DMA1 Stream 0                  */
  .word  DMA1_Stream1_IRQHandler      /* 28: DMA1 Stream 1                  */
  .word  DMA1_Stream2_IRQHandler      /* 29: DMA1 Stream 2                  */
  .word  DMA1_Stream3_IRQHandler      /* 30: DMA1 Stream 3                  */
  .word  DMA1_Stream4_IRQHandler      /* 31: DMA1 Stream 4                  */
  .word  DMA1_Stream5_IRQHandler      /* 32: DMA1 Stream 5                  */
  .word  DMA1_Stream6_IRQHandler      /* 33: DMA1 Stream 6                  */
  .word  ADC_IRQHandler               /* 34: ADC1, ADC2 and ADC3            */
  .word  0                            /* 35: Reserved (no CAN on F411)      */
  .word  0                            /* 36: Reserved                       */
  .word  0                            /* 37: Reserved                       */
  .word  0                            /* 38: Reserved                       */
  .word  EXTI9_5_IRQHandler           /* 39: EXTI Lines 9..5                */
  .word  TIM1_BRK_TIM9_IRQHandler     /* 40: TIM1 Break / TIM9              */
  .word  TIM1_UP_TIM10_IRQHandler     /* 41: TIM1 Update / TIM10            */
  .word  TIM1_TRG_COM_TIM11_IRQHandler/* 42: TIM1 Trigger/COM / TIM11      */
  .word  TIM1_CC_IRQHandler           /* 43: TIM1 Capture Compare           */
  .word  TIM2_IRQHandler              /* 44: TIM2                           */
  .word  TIM3_IRQHandler              /* 45: TIM3                           */
  .word  TIM4_IRQHandler              /* 46: TIM4                           */
  .word  I2C1_EV_IRQHandler           /* 47: I2C1 Event                     */
  .word  I2C1_ER_IRQHandler           /* 48: I2C1 Error                     */
  .word  I2C2_EV_IRQHandler           /* 49: I2C2 Event                     */
  .word  I2C2_ER_IRQHandler           /* 50: I2C2 Error                     */
  .word  SPI1_IRQHandler              /* 51: SPI1                           */
  .word  SPI2_IRQHandler              /* 52: SPI2                           */
  .word  USART1_IRQHandler            /* 53: USART1                         */
  .word  USART2_IRQHandler            /* 54: USART2                         */
  .word  0                            /* 55: Reserved (no USART3 on F411)   */
  .word  EXTI15_10_IRQHandler         /* 56: EXTI Lines 15..10              */
  .word  RTC_Alarm_IRQHandler         /* 57: RTC Alarm (A and B)            */
  .word  OTG_FS_WKUP_IRQHandler       /* 58: USB OTG FS Wakeup              */
  .word  0                            /* 59: Reserved                       */
  .word  0                            /* 60: Reserved                       */
  .word  0                            /* 61: Reserved                       */
  .word  0                            /* 62: Reserved                       */
  .word  DMA1_Stream7_IRQHandler      /* 63: DMA1 Stream 7                  */
  .word  0                            /* 64: Reserved (no FSMC on F411)     */
  .word  SDIO_IRQHandler              /* 65: SDIO                           */
  .word  TIM5_IRQHandler              /* 66: TIM5                           */
  .word  SPI3_IRQHandler              /* 67: SPI3                           */
  .word  0                            /* 68: Reserved                       */
  .word  0                            /* 69: Reserved                       */
  .word  0                            /* 70: Reserved                       */
  .word  0                            /* 71: Reserved                       */
  .word  DMA2_Stream0_IRQHandler      /* 72: DMA2 Stream 0                  */
  .word  DMA2_Stream1_IRQHandler      /* 73: DMA2 Stream 1                  */
  .word  DMA2_Stream2_IRQHandler      /* 74: DMA2 Stream 2                  */
  .word  DMA2_Stream3_IRQHandler      /* 75: DMA2 Stream 3                  */
  .word  DMA2_Stream4_IRQHandler      /* 76: DMA2 Stream 4                  */
  .word  0                            /* 77: Reserved                       */
  .word  0                            /* 78: Reserved                       */
  .word  0                            /* 79: Reserved                       */
  .word  0                            /* 80: Reserved                       */
  .word  0                            /* 81: Reserved                       */
  .word  0                            /* 82: Reserved                       */
  .word  OTG_FS_IRQHandler            /* 83: USB OTG FS                     */
  .word  DMA2_Stream5_IRQHandler      /* 84: DMA2 Stream 5                  */
  .word  DMA2_Stream6_IRQHandler      /* 85: DMA2 Stream 6                  */
  .word  DMA2_Stream7_IRQHandler      /* 86: DMA2 Stream 7                  */
  .word  USART6_IRQHandler            /* 87: USART6                         */
  .word  I2C3_EV_IRQHandler           /* 88: I2C3 Event                     */
  .word  I2C3_ER_IRQHandler           /* 89: I2C3 Error                     */
  .word  0                            /* 90: Reserved                       */
  .word  0                            /* 91: Reserved                       */
  .word  0                            /* 92: Reserved                       */
  .word  0                            /* 93: Reserved                       */
  .word  0                            /* 94: Reserved                       */
  .word  0                            /* 95: Reserved                       */
  .word  0                            /* 96: Reserved                       */
  .word  0                            /* 97: Reserved                       */
  .word  0                            /* 98: Reserved                       */
  .word  SPI4_IRQHandler              /* 99: SPI4                           */
  .word  SPI5_IRQHandler              /*100: SPI5                           */

/*===========================================================================
 * 3.  Reset Handler
 *=========================================================================*/
  .section .text.Reset_Handler
  .weak   Reset_Handler
  .type   Reset_Handler, %function

Reset_Handler:
  /* 1. Copy .data section from Flash to SRAM */
  ldr   r0, =_sdata          /* destination start (SRAM)                   */
  ldr   r1, =_edata          /* destination end   (SRAM)                   */
  ldr   r2, =_sidata         /* source start      (Flash, LMA)             */
  movs  r3, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4

LoopCopyDataInit:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   CopyDataInit

  /* 2. Zero-fill .bss section */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     LoopFillZerobss

FillZerobss:
  str   r3, [r2]
  adds  r2, r2, #4

LoopFillZerobss:
  cmp   r2, r4
  bcc   FillZerobss

  /* 3. Call SystemInit() to configure clocks / FPU */
  bl    SystemInit

  /* 4. Call C++ static constructors (if any) */
  bl    __libc_init_array

  /* 5. Enter application main() */
  bl    main

  /* Should never reach here – loop forever */
InfiniteLoop:
  b     InfiniteLoop

  .size Reset_Handler, .-Reset_Handler

/*===========================================================================
 * 4.  Default / Weak Exception Handlers
 *     ThreadX overrides SVC_Handler, PendSV_Handler, and SysTick_Handler.
 *     All others default to an infinite loop unless the application defines
 *     a real handler with the same name.
 *=========================================================================*/

/* Macro: define a weak infinite-loop handler */
  .macro  WEAK_HANDLER name
  .weak   \name
  .thumb_set \name, Default_Handler
  .endm

  .section .text.Default_Handler, "ax", %progbits
Default_Handler:
Infinite_Loop:
  b     Infinite_Loop
  .size Default_Handler, .-Default_Handler

  /* Core ARM exceptions */
  WEAK_HANDLER  NMI_Handler
  WEAK_HANDLER  HardFault_Handler
  WEAK_HANDLER  MemManage_Handler
  WEAK_HANDLER  BusFault_Handler
  WEAK_HANDLER  UsageFault_Handler
  WEAK_HANDLER  SVC_Handler           /* ThreadX: tx_thread_context_save    */
  WEAK_HANDLER  DebugMon_Handler
  WEAK_HANDLER  PendSV_Handler        /* ThreadX: tx_thread_context_restore */
  WEAK_HANDLER  SysTick_Handler       /* ThreadX: _tx_timer_interrupt        */

  /* STM32F411 peripheral IRQs */
  WEAK_HANDLER  WWDG_IRQHandler
  WEAK_HANDLER  PVD_IRQHandler
  WEAK_HANDLER  TAMP_STAMP_IRQHandler
  WEAK_HANDLER  RTC_WKUP_IRQHandler
  WEAK_HANDLER  FLASH_IRQHandler
  WEAK_HANDLER  RCC_IRQHandler
  WEAK_HANDLER  EXTI0_IRQHandler
  WEAK_HANDLER  EXTI1_IRQHandler
  WEAK_HANDLER  EXTI2_IRQHandler
  WEAK_HANDLER  EXTI3_IRQHandler
  WEAK_HANDLER  EXTI4_IRQHandler
  WEAK_HANDLER  DMA1_Stream0_IRQHandler
  WEAK_HANDLER  DMA1_Stream1_IRQHandler
  WEAK_HANDLER  DMA1_Stream2_IRQHandler
  WEAK_HANDLER  DMA1_Stream3_IRQHandler
  WEAK_HANDLER  DMA1_Stream4_IRQHandler
  WEAK_HANDLER  DMA1_Stream5_IRQHandler
  WEAK_HANDLER  DMA1_Stream6_IRQHandler
  WEAK_HANDLER  ADC_IRQHandler
  WEAK_HANDLER  EXTI9_5_IRQHandler
  WEAK_HANDLER  TIM1_BRK_TIM9_IRQHandler
  WEAK_HANDLER  TIM1_UP_TIM10_IRQHandler
  WEAK_HANDLER  TIM1_TRG_COM_TIM11_IRQHandler
  WEAK_HANDLER  TIM1_CC_IRQHandler
  WEAK_HANDLER  TIM2_IRQHandler
  WEAK_HANDLER  TIM3_IRQHandler
  WEAK_HANDLER  TIM4_IRQHandler
  WEAK_HANDLER  I2C1_EV_IRQHandler
  WEAK_HANDLER  I2C1_ER_IRQHandler
  WEAK_HANDLER  I2C2_EV_IRQHandler
  WEAK_HANDLER  I2C2_ER_IRQHandler
  WEAK_HANDLER  SPI1_IRQHandler
  WEAK_HANDLER  SPI2_IRQHandler
  WEAK_HANDLER  USART1_IRQHandler
  WEAK_HANDLER  USART2_IRQHandler
  WEAK_HANDLER  EXTI15_10_IRQHandler
  WEAK_HANDLER  RTC_Alarm_IRQHandler
  WEAK_HANDLER  OTG_FS_WKUP_IRQHandler
  WEAK_HANDLER  DMA1_Stream7_IRQHandler
  WEAK_HANDLER  SDIO_IRQHandler
  WEAK_HANDLER  TIM5_IRQHandler
  WEAK_HANDLER  SPI3_IRQHandler
  WEAK_HANDLER  DMA2_Stream0_IRQHandler
  WEAK_HANDLER  DMA2_Stream1_IRQHandler
  WEAK_HANDLER  DMA2_Stream2_IRQHandler
  WEAK_HANDLER  DMA2_Stream3_IRQHandler
  WEAK_HANDLER  DMA2_Stream4_IRQHandler
  WEAK_HANDLER  OTG_FS_IRQHandler
  WEAK_HANDLER  DMA2_Stream5_IRQHandler
  WEAK_HANDLER  DMA2_Stream6_IRQHandler
  WEAK_HANDLER  DMA2_Stream7_IRQHandler
  WEAK_HANDLER  USART6_IRQHandler
  WEAK_HANDLER  I2C3_EV_IRQHandler
  WEAK_HANDLER  I2C3_ER_IRQHandler
  WEAK_HANDLER  SPI4_IRQHandler
  WEAK_HANDLER  SPI5_IRQHandler

/*** End of file ***/
