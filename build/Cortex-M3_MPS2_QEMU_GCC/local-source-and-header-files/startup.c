/*
 * FreeRTOS V202012.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CMSIS/CMSDK_CM3.h"
#include "CMSIS/core_cm3.h"

#define UART0_ADDR ((UART_t *)(0x40004000))
#define UART_DR(baseaddr) (*(unsigned int *)(baseaddr))

#define UART_STATE_TXFULL (1 << 0)
#define UART_CTRL_TX_EN (1 << 0)
#define UART_CTRL_RX_EN (1 << 1)

extern void vPortSVCHandler( void );
extern void xPortPendSVHandler( void );
extern void xPortSysTickHandler( void );
extern void xEMACHandler( void );
extern void uart_init();
extern int main( void );
void uart_init( void );

void __attribute__((weak)) EthernetISR (void);

typedef struct UART_t {
    volatile uint32_t DATA;
    volatile uint32_t STATE;
    volatile uint32_t CTRL;
    volatile uint32_t INTSTATUS;
    volatile uint32_t BAUDDIV;
} UART_t;

void Reset_Handler( void )
{
    uart_init();
    main();
}

/* These are volatile to try and prevent the compiler/linker optimising them
away as the variables never actually get used. */
volatile uint32_t r0;
volatile uint32_t r1;
volatile uint32_t r2;
volatile uint32_t r3;
volatile uint32_t r12;
volatile uint32_t lr; /* Link register. */
volatile uint32_t pc; /* Program counter. */
volatile uint32_t psr;/* Program status register. */

void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress )
{
    r0 = pulFaultStackAddress[ 0 ];
    r1 = pulFaultStackAddress[ 1 ];
    r2 = pulFaultStackAddress[ 2 ];
    r3 = pulFaultStackAddress[ 3 ];

    r12 = pulFaultStackAddress[ 4 ];
    lr = pulFaultStackAddress[ 5 ];
    pc = pulFaultStackAddress[ 6 ];
    psr = pulFaultStackAddress[ 7 ];

    printf( "Fault" );

    /* When the following line is hit, the variables contain the register values. */
    for( ;; );
}

static void Default_Handler( void ) __attribute__( ( naked ) );
void Default_Handler(void)
{
    __asm volatile
    (
        "Default_Handler: \n"
        "    ldr r3, NVIC_INT_CTRL_CONST  \n"
        "    ldr r2, [r3, #0]\n"
        "    uxtb r2, r2\n"
        "Infinite_Loop:\n"
        "    b  Infinite_Loop\n"
        ".size  Default_Handler, .-Default_Handler\n"
        ".align 4\n"
        "NVIC_INT_CTRL_CONST: .word 0xe000ed04\n"
    );
}
static void HardFault_Handler( void ) __attribute__( ( naked ) );
void Default_Handler2(void)
{

    __asm volatile
    (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler2_address_const                            \n"
        " bx r2                                                     \n"
        " handler2_address_const: .word prvGetRegistersFromStack    \n"
    );
}

void Default_Handler3(void)
{
    for (;;) { }
}

void Default_Handler4(void)
{
    for (;;) { }
}

void Default_Handler5(void)
{
    for (;;) { }
}

void Default_Handler6(void)
{
    for (;;) { }
}

void Default_Handler13(void)
{
    for (;;) { }
}

extern uint32_t _estack;
const uint32_t* isr_vector[] __attribute__((section(".isr_vector"))) =
{
    (uint32_t*)&_estack,
    (uint32_t*)&Reset_Handler,    // Reset                -15
    (uint32_t*)&Default_Handler,  // NMI_Handler          -14
    (uint32_t*)&Default_Handler2, // HardFault_Handler    -13
    (uint32_t*)&Default_Handler3, // MemManage_Handler    -12
    (uint32_t*)&Default_Handler4, // BusFault_Handler     -11
    (uint32_t*)&Default_Handler5, // UsageFault_Handler   -10
    0, // reserved
    0, // reserved
    0, // reserved
    0, // reserved   -6
    (uint32_t*)&vPortSVCHandler,  // SVC_Handler              -5
    (uint32_t*)&Default_Handler6, // DebugMon_Handler         -4
    0, // reserved
    (uint32_t*)&xPortPendSVHandler,      // PendSV handler    -2
    (uint32_t*)&xPortSysTickHandler,     // SysTick_Handler   -1
    0,                    // uart0 receive 0
    0,                    // uart0 transmit
    0,                    // uart1 receive
    0,                    // uart1 transmit
    0,                    // uart 2 receive
    0,                    // uart 2 transmit
    0,                    // GPIO 0 combined interrupt
    0,                    // GPIO 2 combined interrupt
    0,                    // Timer 0
    0,                    // Timer 1
    0,                    // Dial Timer
    0,                    // SPI0 SPI1
    0,                    // uart overflow 1, 2,3
    (uint32_t*)xEMACHandler, // Ethernet   13

};


__attribute__((naked)) void exit(int status)
{
    // Force qemu to exit using ARM Semihosting
    __asm volatile (
        "mov r1, r0\n"
        "cmp r1, #0\n"
        "bne .notclean\n"
        "ldr r1, =0x20026\n" // ADP_Stopped_ApplicationExit, a clean exit
        ".notclean:\n"
        "movs r0, #0x18\n" // SYS_EXIT
        "bkpt 0xab\n"
        "end: b end\n"
        );
}

void _start( void )
{
    /* Required to link but not used as the reset handler is read from the
    vector table. */
}

/**
 * @brief initializes the UART emulated hardware
 */
void uart_init( void )
{
    /* This is not checking the STATE register to ensure the UART is ready for
    the next character - but that doesn't seem to be an issue in QEMU. */
    UART0_ADDR->BAUDDIV = 16;
    UART0_ADDR->CTRL = UART_CTRL_TX_EN;

    NVIC_DisableIRQ( ETHERNET_IRQn );
    NVIC_SetPriority( ETHERNET_IRQn, 255 );
}


int _write(int file, char *buf, int len)
{
    int todo;

    for (todo = 0; todo < len; todo++)
    {
        UART_DR(UART0_ADDR) = *buf++;
    }
    return len;
}
