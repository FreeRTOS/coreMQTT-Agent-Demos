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
 */

/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "semphr.h"

/*-----------------------------------------------------------*/

/* Dimensions the arrays into which print messages are created. */
#define dlMAX_PRINT_STRING_LENGTH       2048

/* Maximum amount of time to wait for the semaphore that protects the local
buffers and the serial output. */
#define dlMAX_SEMAPHORE_WAIT_TIME		( pdMS_TO_TICKS( 2000UL ) )

int _write( int fd, const void *buffer, unsigned int count );

static char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];
static char cOutputString[ dlMAX_PRINT_STRING_LENGTH ];
static SemaphoreHandle_t xMutex = NULL;

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
    static BaseType_t xMessageNumber = 0;
    int32_t iLength = 0;

    configASSERT( xMutex );

    /* Get exclusive access to _write().  Note this mutex is not given back
     * until the vLoggingPrintf() function is called to ensure the meta data and
     * the log message are printed consecutively. */
    if( xSemaphoreTake( xMutex, dlMAX_SEMAPHORE_WAIT_TIME ) != pdFAIL )
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

        iLength = snprintf( cPrintString, dlMAX_PRINT_STRING_LENGTH, "%s: %s %lu %lu --- ",
                            pcLevel,
                            pcTaskName,
                            xMessageNumber++,
                            ( unsigned long ) xTaskGetTickCount() );

		_write( 0, cPrintString, iLength );
    }

    return iLength;
}
/*-----------------------------------------------------------*/

void vLoggingPrintf( const char * const pcFormat, ... )
{
    int32_t iLength;
    va_list args;
    const char * const pcNewline = "\r\n";

    configASSERT( xMutex );

    /* Only proceed if the preceding call to xLoggingPrintMetadata() obtained
     * the mutex.
     */
    if( xSemaphoreGetMutexHolder( xMutex ) == xTaskGetCurrentTaskHandle() )
    {
        /* There are a variable number of parameters. */
        va_start( args, pcFormat );
		iLength = vsnprintf( cPrintString, dlMAX_PRINT_STRING_LENGTH, pcFormat, args );
		va_end( args );

		_write( 0, cPrintString, iLength );
		_write( 0, pcNewline, strlen( pcNewline ) );

		xSemaphoreGive( xMutex );
    }
}
/*-----------------------------------------------------------*/

/*
 * The TCP/IP stack logging pre-dates the mechanism used by the other libraries
 * and performs a little more work to beutify any IP addresses.  This is called
 * without first calling xLoggingPrintMetadata() so both obtains and releases
 * the mutex within the same file.
 */
void vTCPLoggingPrintf( const char * const pcFormat, ... )
{
    char *pcSource, *pcTarget, *pcBegin;
    int32_t iLength, rc;
    va_list args;
    uint32_t ulIPAddress;
    const char * const pcNewline = "\r\n";

    configASSERT( xMutex);

    if( xSemaphoreTake( xMutex, dlMAX_SEMAPHORE_WAIT_TIME ) != pdFAIL )
    {
        /* There are a variable number of parameters. */
        va_start( args, pcFormat );
		iLength = vsnprintf( cPrintString, dlMAX_PRINT_STRING_LENGTH, pcFormat, args );
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
		iLength = ( BaseType_t ) ( pcTarget - cOutputString );

		_write( 0, cOutputString, iLength );
		_write( 0, pcNewline, strlen( pcNewline ) );

		xSemaphoreGive( xMutex );
    }
}
/*-----------------------------------------------------------*/

void vLoggingInit( void )
{
    /* Create the semaphore used to protect the local buffers and the serial
    port. */
    xMutex = xSemaphoreCreateMutex();
}


