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
 * Logging utility that allows FreeRTOS tasks to log to a UDP port, stdout, and
 * disk file without making any Win32 system calls themselves.
 *
 * Messages logged to a UDP port are sent directly (using FreeRTOS+TCP), but as
 * FreeRTOS tasks cannot make Win32 system calls messages sent to stdout or a
 * disk file are sent via a stream buffer to a Win32 thread which then performs
 * the actual output.
 */

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "semphr.h"

/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/* Demo includes. */
#include "logging.h"

/* Dimensions the arrays into which print messages are created. */
#define dlMAX_PRINT_STRING_LENGTH       2048

/* Maximum amount of time to wait for the semaphore that protects the local
buffers and the serial output. */
#define dlMAX_SEMAPHORE_WAIT_TIME		( pdMS_TO_TICKS( 200UL ) )

int _write( int fd, const void *buffer, unsigned int count );

static char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];
static char cOutputString[ dlMAX_PRINT_STRING_LENGTH ];
static SemaphoreHandle_t xSemaphore = NULL;

void vLoggingPrintf( const char * pcFormat, ... )
{
    char *pcSource, *pcTarget, *pcBegin;
    int32_t xLength, xLength2, rc;
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

    if( ( xSemaphore != NULL ) && ( xSemaphoreTake( xSemaphore, dlMAX_SEMAPHORE_WAIT_TIME ) != pdFAIL ) )
    {
		if( ( xAfterLineBreak == pdTRUE ) && ( strcmp( pcFormat, "\r\n" ) != 0 ) )
		{
			xLength = snprintf( cPrintString, dlMAX_PRINT_STRING_LENGTH, "%lu %lu [%s] ",
								xMessageNumber++,
								( unsigned long ) xTaskGetTickCount(),
								pcTaskName );
			xAfterLineBreak = pdFALSE;
		}
		else
		{
			xLength = 0;
			memset( cPrintString, 0x00, dlMAX_PRINT_STRING_LENGTH );
			xAfterLineBreak = pdTRUE;
		}

		xLength2 = vsnprintf( cPrintString + xLength, dlMAX_PRINT_STRING_LENGTH - xLength, pcFormat, args );

		if( xLength2 < 0 )
		{
			/* Clean up. */
			xLength2 = dlMAX_PRINT_STRING_LENGTH - 1 - xLength;
			cPrintString[ dlMAX_PRINT_STRING_LENGTH - 1 ] = '\0';
		}

		xLength += xLength2;
		va_end( args );

		/* For ease of viewing, copy the string into another buffer, converting
		 * IP addresses to dot notation on the way. */
		pcSource = cPrintString;
		pcTarget = cOutputString;

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
		xLength = ( BaseType_t ) ( pcTarget - cOutputString );

		_write( 0, cOutputString, xLength );

		xSemaphoreGive( xSemaphore );
    }
}
/*-----------------------------------------------------------*/

void vLoggingInit( BaseType_t xLogToStdout,
                   BaseType_t xLogToFile,
                   BaseType_t xLogToUDP,
                   uint32_t ulRemoteIPAddress,
                   uint16_t usRemotePort )
{
    /* Not used in this port.  Cast parameters to void to prevent compiler
    warnings about unused parameters. */
    ( void ) xLogToStdout;
    ( void ) xLogToFile;
    ( void ) xLogToUDP;
    ( void ) ulRemoteIPAddress;
    ( void ) usRemotePort;

    /* Create the semaphore used to protect the local buffers and the serial
    port. */
    xSemaphore = xSemaphoreCreateMutex();
}


