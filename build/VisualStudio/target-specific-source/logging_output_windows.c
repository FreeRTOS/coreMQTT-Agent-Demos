/*
 * Lab-Project-coreMQTT-Agent 201215
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
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*
 * This logging example uses two function calls per logged message.  First
 * xLoggingPrintMetadata() attempts to obtain a mutex that grants it access to
 * the output port before outputting metadata about the log.  Second
 * vLoggingPrintf() writes the log message itself before releasing the mutex.
 *
 * The prototypes for these functions are in demo_config.h so they can be
 * adjusted for more or less metadata as required.
 *
 * FreeRTOS tasks should not access the console directly as to do so can block
 * the containing Windows thread outside of the FreeRTOS scheduler's control.
 * Therefore log messages are sent via a circular buffer to a Windows thread that
 * is outside of the FreeRTOS kernel's control, and output to the concole from
 * there.
 */
/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <ctype.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "semphr.h"

/* TCP/IP includes - required to ensure the TCP stack's SteamBuffer_t is used
 * rather than the kernel's one.  Confusingly the TCP's definition is the one
 * defined with FreeRTOS_Stream_Buffer.h. */
#include "FreeRTOS_Stream_Buffer.h"

/*-----------------------------------------------------------*/

/* Dimensions the arrays into which print messages are created. */
#define dlMAX_PRINT_STRING_LENGTH       2048

/* The size of the stream buffer used to pass messages from FreeRTOS tasks to
 * the Win32 thread that is responsible for making any Win32 system calls that
 * are necessary to output the text. */
#define dlLOGGING_STREAM_BUFFER_SIZE    32768

/* Maximum amount of time to wait for the semaphore that protects the local
 * buffers and the serial output. */
#define dlMAX_SEMAPHORE_WAIT_TIME       ( pdMS_TO_TICKS( 2000UL ) )

/*-----------------------------------------------------------*/

/*
 * Before the scheduler is started this function is called directly.  After the
 * scheduler has started it is called from the Windows thread dedicated to
 * outputting log messages.  Only the windows thread actually performs the
 * writing so as not to disrupt the simulation by making Windows system calls
 * from FreeRTOS tasks.
 */
static void prvLoggingFlushBuffer( void );

/*
 * The windows thread that performs the actual writing of messages that require
 * Win32 system calls.  Only the windows thread can make system calls so as not
 * to disrupt the simulation by making Windows calls from FreeRTOS tasks.
 */
static DWORD WINAPI prvWin32LoggingThread( void * pvParam );

/*-----------------------------------------------------------*/

/* Windows event used to wake the Win32 thread which performs any logging that
 * needs Win32 system calls. */
static void * pvLoggingThreadEvent = NULL;

/* Circular buffer used to pass messages from the FreeRTOS tasks to the Win32
 * thread that is responsible for making Win32 calls (when stdout or a disk log is
 * used). */
static StreamBuffer_t * xLogStreamBuffer = NULL;

/* When true prints are performed directly.  After start up xDirectPrint is set
 * to pdFALSE - at which time prints that require Win32 system calls are done by
 * the Win32 thread responsible for logging. */
BaseType_t xDirectPrint = pdTRUE;

/* Used to manage access to the stream buffer. */
static SemaphoreHandle_t xStreamBufferMutex = NULL;

/* Number of bytes used to pass the message length in the circular buffer. */
const int32_t iMessageLengthBytes = 4;

/*-----------------------------------------------------------*/

/*
 * The prototype for this function, and the macros that call this function, are
 * both in demo_config.h.  Update the macros and prototype to pass in additional
 * meta data if required - for example the name of the function that called the
 * log message can be passed in by adding an additional parameter to the function
 * then updating the macro to pass __FUNCTION__ as the parameter value.  See the
 * comments in demo_config.h for more information.
 */
int32_t xLoggingPrintMetadata( const char * const pcLevel )
{
    const char * pcTaskName;
    const char * pcNoTask = "None";
    char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];
    static BaseType_t xMessageNumber = 0;
    int32_t iLength = 0;
    size_t xSpaceInBuffer;

    configASSERT( xStreamBufferMutex );

    /* Get exclusive access to _write().  Note this mutex is not given back
     * until the vLoggingPrintf() function is called to ensure the meta data and
     * the log message are printed consecutively. */
    if( xSemaphoreTake( xStreamBufferMutex, dlMAX_SEMAPHORE_WAIT_TIME ) != pdFAIL )
    {
        /* Additional info to place at the start of the log. */
        if( xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED )
        {
            pcTaskName = pcTaskGetName( NULL );
        }
        else
        {
            pcTaskName = pcNoTask;
        }

        /* Write the string at an offset that allows room for the length of the
         * message to be written at the beginning of the buffer. */
        iLength = snprintf( cPrintString + iMessageLengthBytes, dlMAX_PRINT_STRING_LENGTH - iMessageLengthBytes, "%s: %s %lu %lu --- ",
                            pcLevel,
                            pcTaskName,
                            xMessageNumber++,
                            ( unsigned long ) xTaskGetTickCount() );

        /* Write the message length in the beginning of the buffer. */
        memcpy( cPrintString, &iLength, iMessageLengthBytes );
        iLength += iMessageLengthBytes;

        xSpaceInBuffer = uxStreamBufferGetSpace( xLogStreamBuffer );

        if( xSpaceInBuffer >= ( size_t ) iLength )
        {
            /* The calling task can access the stream buffer because it holds the
             * mutex but must also raise its priority to be above the priority of the
             * windows thread that reads from the stream buffer. */
            uxStreamBufferAdd( xLogStreamBuffer, 0, ( const uint8_t * ) cPrintString, ( size_t ) iLength );

            /* xDirectPrint is initialized to pdTRUE, and while it remains true the
             * logging output function is called directly.  When the system is running
             * the output function cannot be called directly because it would get
             * called from both FreeRTOS tasks and Win32 threads - so instead wake the
             * Win32 thread responsible for the actual output. */
            if( xDirectPrint != pdFALSE )
            {
                /* While starting up, the thread which calls prvWin32LoggingThread()
                 * is not running yet and xDirectPrint will be pdTRUE. */
                prvLoggingFlushBuffer();
            }
            else if( pvLoggingThreadEvent != NULL )
            {
                /* While running, wake up prvWin32LoggingThread() to send the
                 * logging data. */
                SetEvent( pvLoggingThreadEvent );
            }
        }
    }

    return iLength;
}
/*-----------------------------------------------------------*/

void vLoggingPrintf( const char * const pcFormat,
                     ... )
{
    size_t xSpaceInBuffer;
    int32_t iLength;
    va_list args;
    const char * const pcNewline = "\r\n";
    const int32_t iNewlineStringLength = ( int32_t ) strlen( pcNewline );
    char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];

    configASSERT( xStreamBufferMutex );

    /* Only proceed if the preceding call to xLoggingPrintMetadata() obtained
     * the mutex. */
    if( xSemaphoreGetMutexHolder( xStreamBufferMutex ) == xTaskGetCurrentTaskHandle() )
    {
        /* There are a variable number of parameters.  Write the message at
         * an offset that allows the length of the message to be written at the
         * start of the buffer. */
        va_start( args, pcFormat );
        iLength = vsnprintf( cPrintString + iMessageLengthBytes, dlMAX_PRINT_STRING_LENGTH - iMessageLengthBytes - iNewlineStringLength, pcFormat, args );
        va_end( args );

        /* Add the newline character to the end of the message. */
        strcat( cPrintString + iMessageLengthBytes, pcNewline );
        iLength += iNewlineStringLength;

        /* Write the message length in the beginning of the buffer. */
        memcpy( cPrintString, &iLength, iMessageLengthBytes );
        iLength += iMessageLengthBytes;

        xSpaceInBuffer = uxStreamBufferGetSpace( xLogStreamBuffer );

        if( xSpaceInBuffer >= ( size_t ) iLength )
        {
            /* The calling task can access the stream buffer because it holds the
             * mutex but must also raise its priority to be above the priority of the
             * windows thread that reads from the stream buffer. */
            uxStreamBufferAdd( xLogStreamBuffer, 0, ( const uint8_t * ) cPrintString, ( size_t ) iLength );

            /* xDirectPrint is initialized to pdTRUE, and while it remains true the
             * logging output function is called directly.  When the system is running
             * the output function cannot be called directly because it would get
             * called from both FreeRTOS tasks and Win32 threads - so instead wake the
             * Win32 thread responsible for the actual output. */
            if( xDirectPrint != pdFALSE )
            {
                /* While starting up, the thread which calls prvWin32LoggingThread()
                 * is not running yet and xDirectPrint will be pdTRUE. */
                prvLoggingFlushBuffer();
            }
            else if( pvLoggingThreadEvent != NULL )
            {
                /* While running, wake up prvWin32LoggingThread() to send the
                 * logging data. */
                SetEvent( pvLoggingThreadEvent );
            }
        }

        xSemaphoreGive( xStreamBufferMutex );
    }
}
/*-----------------------------------------------------------*/

/*
 * The TCP/IP stack logging pre-dates the mechanism used by the other libraries
 * and performs a little more work to beutify any IP addresses.  This is called
 * without first calling xLoggingPrintMetadata() so both obtains and releases
 * the mutex within the same file.
 */
void vTCPLoggingPrintf( const char * pcFormat,
                        ... )
{
    char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];
    char cOutputString[ dlMAX_PRINT_STRING_LENGTH ];
    char * pcSource, * pcTarget, * pcBegin;
    size_t xSpaceInBuffer, rc;
    int32_t iLength;
    static BaseType_t xMessageNumber = 0;
    static BaseType_t xAfterLineBreak = pdTRUE;
    va_list args;
    uint32_t ulIPAddress;
    const char * pcTaskName;
    const char * pcNoTask = "None";

    /* There are a variable number of parameters. */
    va_start( args, pcFormat );

    /* Additional info to place at the start of the log. */
    if( xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED )
    {
        pcTaskName = pcTaskGetName( NULL );
    }
    else
    {
        pcTaskName = pcNoTask;
    }

    /* There are a variable number of parameters. */
    va_start( args, pcFormat );
    vsnprintf( cPrintString, dlMAX_PRINT_STRING_LENGTH, pcFormat, args );
    va_end( args );

    /* For ease of viewing, copy the string into another buffer, converting
     * IP addresses to dot notation on the way.  Leave space at the beginning
     * of the buffer to hold the message length. */
    pcSource = cPrintString;
    pcTarget = cOutputString + iMessageLengthBytes;

    while( ( *pcSource ) != '\0' )
    {
        *pcTarget = *pcSource;
        pcTarget++;
        pcSource++;

        /* Look forward for an IP address denoted by 'ip'. */
        if( ( isxdigit( ( int ) pcSource[ 0 ] ) != pdFALSE ) && ( pcSource[ 1 ] == 'i' ) && ( pcSource[ 2 ] == 'p' ) )
        {
            *pcTarget = *pcSource;
            pcTarget++;
            *pcTarget = '\0';
            pcBegin = pcTarget - 8;

            while( ( pcTarget > pcBegin ) && ( isxdigit( ( int ) pcTarget[ -1 ] ) != pdFALSE ) )
            {
                pcTarget--;
            }

            sscanf( pcTarget, "%8X", ( unsigned int * ) &ulIPAddress );
            rc = sprintf( pcTarget, "%lu.%lu.%lu.%lu",
                          ( unsigned long ) ( ulIPAddress >> 24UL ),
                          ( unsigned long ) ( ( ulIPAddress >> 16UL ) & 0xffUL ),
                          ( unsigned long ) ( ( ulIPAddress >> 8UL ) & 0xffUL ),
                          ( unsigned long ) ( ulIPAddress & 0xffUL ) );
            pcTarget += rc;
            pcSource += 3; /* skip "<n>ip" */
        }
    }

    /* How far through the buffer was written? */
    iLength = ( int32_t ) ( pcTarget - cOutputString );

    /* Adjust for the fact that space was left at the beginning of the buffer
     * to hold the message length. */
    iLength -= iMessageLengthBytes;

    /* Write the message length in the beginning of the buffer. */
    memcpy( cOutputString, &iLength, iMessageLengthBytes );


    configASSERT( xLogStreamBuffer );

    /* How much space is in the buffer? */
    if( xSemaphoreTake( xStreamBufferMutex, dlMAX_SEMAPHORE_WAIT_TIME ) != pdFAIL )
    {
        xSpaceInBuffer = uxStreamBufferGetSpace( xLogStreamBuffer );

        if( xSpaceInBuffer >= ( size_t ) ( iLength + iMessageLengthBytes ) )
        {
            /* The calling task can access the stream buffer because it holds the
             * mutex but must also raise its priority to be above the priority of the
             * windows thread that reads from the stream buffer. */
            uxStreamBufferAdd( xLogStreamBuffer, 0, ( const uint8_t * ) cOutputString, ( size_t ) ( iLength + iMessageLengthBytes ) );

            /* xDirectPrint is initialized to pdTRUE, and while it remains true the
             * logging output function is called directly.  When the system is running
             * the output function cannot be called directly because it would get
             * called from both FreeRTOS tasks and Win32 threads - so instead wake the
             * Win32 thread responsible for the actual output. */
            if( xDirectPrint != pdFALSE )
            {
                /* While starting up, the thread which calls prvWin32LoggingThread()
                 * is not running yet and xDirectPrint will be pdTRUE. */
                prvLoggingFlushBuffer();
            }
            else if( pvLoggingThreadEvent != NULL )
            {
                /* While running, wake up prvWin32LoggingThread() to send the
                 * logging data. */
                SetEvent( pvLoggingThreadEvent );
            }
        }

        xSemaphoreGive( xStreamBufferMutex );
    }
}
/*-----------------------------------------------------------*/

void vLoggingInit( void )
{
    HANDLE Win32Thread;

    /* Can only be called before the scheduler has started. */
    configASSERT( xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED );

    /* Create the semaphore used to protect the local buffers and the serial
     * port. */
    xStreamBufferMutex = xSemaphoreCreateMutex();

    /* Printing system calls cannot be made from FreeRTOS tasks so create a
     * stream buffer to pass the messages to a Win32 thread, then create the
     * Win32 thread itself, along with a Win32 event that can be used to
     * unblock the thread. */

    xLogStreamBuffer = ( StreamBuffer_t * ) malloc( sizeof( *xLogStreamBuffer ) - sizeof( xLogStreamBuffer->ucArray ) + dlLOGGING_STREAM_BUFFER_SIZE + 1 );
    configASSERT( xLogStreamBuffer );
    memset( xLogStreamBuffer, '\0', sizeof( *xLogStreamBuffer ) - sizeof( xLogStreamBuffer->ucArray ) );
    xLogStreamBuffer->LENGTH = dlLOGGING_STREAM_BUFFER_SIZE + 1;

    /* Create the Windows event. */
    pvLoggingThreadEvent = CreateEvent( NULL, FALSE, TRUE, "StdoutLoggingEvent" );

    /* Create the thread itself. */
    Win32Thread = CreateThread(
        NULL,                  /* Pointer to thread security attributes. */
        0,                     /* Initial thread stack size, in bytes. */
        prvWin32LoggingThread, /* Pointer to thread function. */
        NULL,                  /* Argument for new thread. */
        0,                     /* Creation flags. */
        NULL );

    configASSERT( Win32Thread );

    if( Win32Thread != NULL )
    {
        /* Use the cores that are not used by the FreeRTOS tasks. */
        SetThreadAffinityMask( Win32Thread, ~0x01u );
        SetThreadPriorityBoost( Win32Thread, TRUE );
        SetThreadPriority( Win32Thread, THREAD_PRIORITY_IDLE );
    }
}
/*-----------------------------------------------------------*/

static void prvLoggingFlushBuffer( void )
{
    size_t xLength;
    char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];

    /* Is there more than the length value stored in the circular buffer
     * used to pass data from the FreeRTOS simulator into this Win32 thread? */
    while( uxStreamBufferGetSize( xLogStreamBuffer ) > sizeof( xLength ) )
    {
        memset( cPrintString, 0x00, dlMAX_PRINT_STRING_LENGTH );
        uxStreamBufferGet( xLogStreamBuffer, 0, ( uint8_t * ) &xLength, sizeof( xLength ), pdFALSE );
        configASSERT( xLength < dlMAX_PRINT_STRING_LENGTH );
        uxStreamBufferGet( xLogStreamBuffer, 0, ( uint8_t * ) cPrintString, xLength, pdFALSE );

        /* Write the message to stdout. */
        _write( _fileno( stdout ), cPrintString, strlen( cPrintString ) );
    }
}
/*-----------------------------------------------------------*/

static DWORD WINAPI prvWin32LoggingThread( void * pvParameter )
{
    const DWORD xMaxWait = 1000;

    ( void ) pvParameter;

    /* From now on, prvLoggingFlushBuffer() will only be called from this
     * Windows thread */
    xDirectPrint = pdFALSE;

    for( ; ; )
    {
        /* Wait to be told there are message waiting to be logged. */
        WaitForSingleObject( pvLoggingThreadEvent, xMaxWait );

        /* Write out all waiting messages. */
        prvLoggingFlushBuffer();
    }
}
/*-----------------------------------------------------------*/
