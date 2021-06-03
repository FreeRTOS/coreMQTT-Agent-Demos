/*
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
 */

/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* FreeRTOS Socket wrapper include. */
#include "sockets_wrapper.h"

/* Transport interface include. */
#include "using_plaintext.h"

PlaintextTransportStatus_t Plaintext_FreeRTOS_Connect( NetworkContext_t * pNetworkContext,
                                                       const char * pHostName,
                                                       uint16_t port,
                                                       uint32_t receiveTimeoutMs,
                                                       uint32_t sendTimeoutMs )
{
    PlaintextTransportStatus_t plaintextStatus = PLAINTEXT_TRANSPORT_SUCCESS;
    BaseType_t socketStatus = 0;

    if( ( pNetworkContext == NULL ) || ( pHostName == NULL ) )
    {
        LogError( ( "Invalid input parameter(s): Arguments cannot be NULL. pNetworkContext=%p, "
                    "pHostName=%p.",
                    pNetworkContext,
                    pHostName ) );
        plaintextStatus = PLAINTEXT_TRANSPORT_INVALID_PARAMETER;
    }
    else
    {
        /* Establish a TCP connection with the server. */
        socketStatus = Sockets_Connect( &( pNetworkContext->tcpSocket ),
                                        pHostName,
                                        port,
                                        receiveTimeoutMs,
                                        sendTimeoutMs );

        /* A non zero status is an error. */
        if( socketStatus != 0 )
        {
            LogError( ( "Failed to connect to %s with error %d.",
                        pHostName,
                        socketStatus ) );
            plaintextStatus = PLAINTEXT_TRANSPORT_CONNECT_FAILURE;
        }
    }

    return plaintextStatus;
}

PlaintextTransportStatus_t Plaintext_FreeRTOS_Disconnect( const NetworkContext_t * pNetworkContext )
{
    PlaintextTransportStatus_t plaintextStatus = PLAINTEXT_TRANSPORT_SUCCESS;

    if( pNetworkContext == NULL )
    {
        LogError( ( "pNetworkContext cannot be NULL." ) );
        plaintextStatus = PLAINTEXT_TRANSPORT_INVALID_PARAMETER;
    }
    else if( pNetworkContext->tcpSocket == FREERTOS_INVALID_SOCKET )
    {
        LogError( ( "pNetworkContext->tcpSocket cannot be an invalid socket." ) );
        plaintextStatus = PLAINTEXT_TRANSPORT_INVALID_PARAMETER;
    }
    else
    {
        /* Call socket disconnect function to close connection. */
        Sockets_Disconnect( pNetworkContext->tcpSocket );
    }

    return plaintextStatus;
}

int32_t Plaintext_FreeRTOS_recv( NetworkContext_t * pNetworkContext,
                                 void * pBuffer,
                                 size_t bytesToRecv )
{
    int32_t socketStatus;

    /* The TCP socket may have a receive block time.  If bytesToRecv is greater 
     * than 1 then a frame is likely already part way through reception and 
     * blocking to wait for the desired number of bytes to be available is the
     * most efficient thing to do.  If bytesToRecv is 1 then this may be a 
     * speculative call to read to find the start of a new frame, in which case 
     * blocking is not desirable as it could block an entire protocol agent 
     * task for the duration of the read block time and therefore negatively 
     * impact performance.  So if bytesToRecv is 1 then don't call recv unless 
     * it is known that bytes are already available. */
    if( ( bytesToRecv > 1 ) || ( FreeRTOS_recvcount( pNetworkContext->tcpSocket ) > 0 ) )
    {
        socketStatus = FreeRTOS_recv( pNetworkContext->tcpSocket, pBuffer, bytesToRecv, 0 );
    }
    else
    {
        socketStatus = 0;
    }

    return socketStatus;
}

int32_t Plaintext_FreeRTOS_send( NetworkContext_t * pNetworkContext,
                                 const void * pBuffer,
                                 size_t bytesToSend )
{
    int32_t socketStatus = 0;

    socketStatus = FreeRTOS_send( pNetworkContext->tcpSocket, pBuffer, bytesToSend, 0 );

    if( socketStatus == -pdFREERTOS_ERRNO_ENOSPC )
    {
        /* The TCP buffers could not accept any more bytes so zero bytes were sent
         * but this is not necessarily an error that should cause a disconnect
         * unless it persists. */
        socketStatus = 0;
    }

    #if ( configUSE_PREEMPTION == 1 )
        {
            /* Allow packet to be sent instantly. Fixes a bug in MQTT_Connect() where
             * the CONNECT packet is not sent before the task waits for the CONNACK from the broker,
             * resulting in return code MQTTNoDataAvailable due to timeout.
             */
            taskYIELD();
        }
    #endif

    return socketStatus;
}
