/*
 * FreeRTOS Kernel V10.3.0
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
 */

/*
 * This demo shows how to use coreMQTT in a multithreaded environment - it does not
 * yet go as far as encapsulating the MQTT library within its own agent (or daemon)
 * task - although the MQTTAgent_CommandLoop() function demonstrates how that might be done.
 * In this task MQTTAgent_CommandLoop() is only executed from a single thread and is the
 * only function that is allowed to use the coreMQTT API directly.  Anything else
 * needing to interact with the coreMQTT API does so by posting commands to
 * MQTTAgent_CommandLoop() via a queue.  Future coreMQTT releases will build an agent into
 * the library itself, and then encapsulate the queues into the implementation of a
 * thread safe coreMQTT API.
 *
 * To use this demo with TLS set democonfigUSE_TLS to 1.  To use this demo without
 * TLS (so plain text) set democonfigUSE_TLS to 0.  democonfigUSE_TLS is defined
 * in demo_config.h.
 *
 *!!! Plain text connections are only used for ease of demonstration.  Do not send
 *!!! sensitive data on unencrypted connections.  Production devices should used
 *!!! mutually authenticated and encrypted connections.
 *
 * There are four tasks to note in this demo:
 *  - prvMQTTDemoTask() manages multiple iterations of the demo.  Each iteration
 *    creates the other tasks, calls MQTTAgent_CommandLoop() to handle the MQTT traffic,
 *    then cleans up ready for the next iteration.
 *  - prvSyncPublishTask() which demonstrates synchronous publishes. The task creates
 *    a series of publish operations that are sent over the command queue to be
 *    processed by MQTTAgent_CommandLoop(), waiting for each publish to complete before
 *    sending the next.
 *  - prvAsyncPublishTask() which demonstrates asynchronous publishes. Like
 *    prvSyncPublishTask(), the task creates a series of publish operations that are
 *    sent over the command queue to be processed by MQTTAgent_CommandLoop(), but unlike
 *    prvSyncPublishTask() this task does not wait for each publish to be complete
 *    until after all the publish commands are sent.  Note that the distinction
 *    between synchronous and asynchronous publishes is only in the behavior of the
 *    task, not in the actual publish command.
 *  - prvSubscribeTask() which creates an MQTT subscription to a topic filter
 *    matching the topics published on by the two publishing tasks, and in doing so,
 *    ensures the demo received a publish command back for each publish command it
 *    sends. It loops while waiting for publish messages to be received.
 *
 * Tasks can have queues to hold received publish messages, and the command task
 * will push incoming publishes to the queue of each task that is subscribed to
 * the incoming topic.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* Demo Specific configs. */
#include "demo_config.h"

/* MQTT library includes. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

/* MQTT agent include. */
#include "freertos_mqtt_agent.h"

/* Exponential backoff retry include. */
#include "exponential_backoff.h"


/* Transport interface include. */
#if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 )
    #include "using_mbedtls.h"
#else
    #include "using_plaintext.h"

#endif

/**
 * These configuration settings are required to run the demo.
 */

/**
 * @brief Timeout for receiving CONNACK packet in milliseconds.
 */
#define mqttexampleCONNACK_RECV_TIMEOUT_MS           ( 1000U )

/**
 * @brief The maximum time interval in seconds which is allowed to elapse
 *  between two Control Packets.
 *
 *  It is the responsibility of the Client to ensure that the interval between
 *  Control Packets being sent does not exceed the this Keep Alive value. In the
 *  absence of sending any other Control Packets, the Client MUST send a
 *  PINGREQ Packet.
 */
#define mqttexampleKEEP_ALIVE_INTERVAL_SECONDS       ( 60U )

/**
 * @brief Transport timeout in milliseconds for transport send and receive. Used
 * while connecting, after which the receive timeout is set to 0 ready for use in
 * the agent task.
 */
#define mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS    ( 1000 )

/**
 * @brief Milliseconds per second.
 */
#define mqttexampleMILLISECONDS_PER_SECOND           ( 1000U )

/**
 * @brief Milliseconds per FreeRTOS tick.
 */
#define mqttexampleMILLISECONDS_PER_TICK             ( mqttexampleMILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

/**
 * @brief Ticks to wait for task notifications.
 */
#define mqttexampleDEMO_TICKS_TO_WAIT                pdMS_TO_TICKS( 10000 )

/**
 * @brief Timeout for MQTT_ProcessLoop function in milliseconds.
 *
 * This demo uses no delay for the process loop, so each invocation will run
 * one iteration, and will only receive a single packet. However, if there is
 * no data available on the socket, the entire socket timeout value will elapse.
 */
#define mqttexamplePROCESS_LOOP_TIMEOUT_MS           ( 0U )

/**
 * @brief Size of statically allocated buffers for holding topic names and payloads.
 */
#define mqttexampleDEMO_BUFFER_SIZE                 100

/**
 * @brief The maximum number of loop iterations to wait before declaring failure.
 *
 * Each `while` loop waiting for a task notification will wait for a total
 * number of ticks equal to `mqttexampleDEMO_TICKS_TO_WAIT` * this number of
 * iterations before the loop exits.
 *
 * @note This value should not be too small, as the reason for a long loop
 * may be a loss of network connection.
 */
#define mqttexampleMAX_WAIT_ITERATIONS               ( 20 )

/**
 * @brief The number of subscribe-publish tasks to create.
 */
#define mqttexampleNUM_SUBSCRIBE_PUBLISH_TASKS       3

/*-----------------------------------------------------------*/

struct CommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
};

/**
 * @brief An element for a task's response queue for received publishes.
 *
 * @note Since elements are copied to queues, this struct needs to hold
 * buffers for the payload and topic of incoming publishes, as the original
 * pointers are out of scope. When processing a publish from this struct,
 * the `pcTopicNameBuf` and `pcPayloadBuf` pointers need to be set to point to the
 * static buffers in this struct.
 */
typedef struct publishElement
{
    MQTTPublishInfo_t xPublishInfo;
    uint8_t pcPayloadBuf[ mqttexampleDEMO_BUFFER_SIZE ];
    uint8_t pcTopicNameBuf[ mqttexampleDEMO_BUFFER_SIZE ];
} PublishElement_t;

/*-----------------------------------------------------------*/


/**
 * @brief Initializes an MQTT context, including transport interface and
 * network buffer.
 *
 * @return `MQTTSuccess` if the initialization succeeds, else `MQTTBadParameter`.
 */
static MQTTStatus_t prvMQTTInit( void );

/**
 * @brief Sends an MQTT Connect packet over the already connected TCP socket.
 *
 * @param[in] pxMQTTContext MQTT context pointer.
 * @param[in] xCleanSession If a clean session should be established.
 *
 * @return `MQTTSuccess` if connection succeeds, else appropriate error code
 * from MQTT_Connect.
 */
static MQTTStatus_t prvMQTTConnect( bool xCleanSession );

/**
 * @brief Form a TCP connection to a server.
 *
 * @param[in] pxNetworkContext Network context.
 *
 * @return `pdPASS` if connection succeeds, else `pdFAIL`.
 */
static BaseType_t prvSocketConnect( NetworkContext_t * pxNetworkContext );

/**
 * @brief Disconnect a TCP connection.
 *
 * @param[in] pxNetworkContext Network context.
 *
 * @return `pdPASS` if disconnect succeeds, else `pdFAIL`.
 */
static BaseType_t prvSocketDisconnect( NetworkContext_t * pxNetworkContext );

/**
 * @brief Callback for adding a process loop call to a command queue, when data
 * is available on a socket.
 *
 * @param[in] pxSocket Socket with data, unused.
 */
static void prvMQTTClientSocketWakeupCallback( Socket_t pxSocket );

/**
 * @brief Copy an incoming publish to a response queue.
 *
 * @param[in] pxPublishInfo Info of incoming publish.
 * @param[in] pxResponseQueue Queue to which the publish is copied.
 */
static void prvCopyPublishToQueue( MQTTPublishInfo_t * pxPublishInfo,
                                   void * pxResponseQueue );

/**
 * @brief Task used to run the MQTT agent.
 *
 * This task calls MQTTAgent_CommandLoop() in a loop, until MQTTAgent_Terminate()
 * is called. If an error occurs in the command loop, then it will reconnect the
 * TCP and MQTT connections.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
static void prvMQTTAgentTask( void * pvParameters );

/**
 * @brief The main task used in the MQTT demo.
 *
 * After creating the publisher and subscriber tasks, this task will enter a
 * loop, processing commands from the command queue and calling the MQTT API.
 * After the termination command is received on the command queue, the task
 * will break from the loop.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
static void prvMQTTDemoTask( void * pvParameters );

/**
 * @brief The timer query function provided to the MQTT context.
 *
 * @return Time in milliseconds.
 */
static uint32_t prvGetTimeMs( void );

//_RB_ To document.
static BaseType_t prvCreateMQTTAgent( void );

/*-----------------------------------------------------------*/

/**
 * @brief Global MQTT context handle - use context 0.
 */
const MQTTContextHandle_t xMQTTContextHandle = 0;

/**
 * @brief Global Network context.
 */
static NetworkContext_t xNetworkContext;

/**
 * @brief Handle for prvMQTTDemoTask.
 */
static TaskHandle_t xMainTask;

/**
 * @brief Handle for prvMQTTAgentTask.
 */
static TaskHandle_t xAgentTask;

/**
 * @brief Response queue for publishes received on non-subscribed topics.
 */
static QueueHandle_t xDefaultResponseQueue;

/**
 * @brief Global entry time into the application to use as a reference timestamp
 * in the #prvGetTimeMs function. #prvGetTimeMs will always return the difference
 * between the current time and the global entry time. This will reduce the chances
 * of overflow for the 32 bit unsigned integer used for holding the timestamp.
 */
static uint32_t ulGlobalEntryTimeMs;

/*-----------------------------------------------------------*/

/*
 * @brief Create the task that demonstrates the MQTT Connection sharing demo.
 */
void vStartSimpleMQTTDemo( void )
{
    /* This example uses one application task to process the command queue for
     * MQTT operations, and creates additional tasks to add operations to that
     * queue. */
    xTaskCreate( prvMQTTDemoTask,          /* Function that implements the task. */
                 "DemoTask",               /* Text name for the task - only used for debugging. */
                 democonfigDEMO_STACKSIZE, /* Size of stack (in words, not bytes) to allocate for the task. */
                 NULL,                     /* Task parameter - not used in this case. */
                 tskIDLE_PRIORITY + 1,     /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                 &xMainTask );             /* Used to pass out a handle to the created task. */
}
/*-----------------------------------------------------------*/

static MQTTStatus_t prvMQTTInit( void )
{
    TransportInterface_t xTransport;
    MQTTStatus_t xReturn;
    const MQTTContextHandle_t xGlobalMQTTContextHandle = 0;

    /* Fill in Transport Interface send and receive function pointers. */
    xTransport.pNetworkContext = &xNetworkContext;
    #if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 )
        xTransport.send = TLS_FreeRTOS_send;
        xTransport.recv = TLS_FreeRTOS_recv;
    #else
        xTransport.send = Plaintext_FreeRTOS_send;
        xTransport.recv = Plaintext_FreeRTOS_recv;
    #endif

    /* Initialize MQTT library. */
    xReturn = MQTTAgent_Init( xGlobalMQTTContextHandle,
                              &xTransport,
                              prvGetTimeMs,
                              prvCopyPublishToQueue, /* Callback to execute if receiving publishes on unexpected topics. */
                              xDefaultResponseQueue ); /* Context to pass into the callback. */

    return xReturn;
}

static MQTTStatus_t prvMQTTConnect( bool xCleanSession )
{
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent = false;

    /* Many fields are not used in this demo so start with everything at 0. */
    memset( &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = xCleanSession;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = democonfigCLIENT_IDENTIFIER;/*_RB_ should use functions to obtain these constants. */
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( democonfigCLIENT_IDENTIFIER );

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value. In the absence of sending any other Control
     * Packets, the Client MUST send a PINGREQ Packet. */
    xConnectInfo.keepAliveSeconds = mqttexampleKEEP_ALIVE_INTERVAL_SECONDS;

    /* Append metrics when connecting to the AWS IoT Core broker. */
    #ifdef democonfigUSE_AWS_IOT_CORE_BROKER
        #ifdef democonfigCLIENT_USERNAME
            xConnectInfo.pUserName = CLIENT_USERNAME_WITH_METRICS;
            xConnectInfo.userNameLength = ( uint16_t ) strlen( CLIENT_USERNAME_WITH_METRICS );
            xConnectInfo.pPassword = democonfigCLIENT_PASSWORD;
            xConnectInfo.passwordLength = ( uint16_t ) strlen( democonfigCLIENT_PASSWORD );
        #else
            xConnectInfo.pUserName = AWS_IOT_METRICS_STRING;
            xConnectInfo.userNameLength = AWS_IOT_METRICS_STRING_LENGTH;
            /* Password for authentication is not used. */
            xConnectInfo.pPassword = NULL;
            xConnectInfo.passwordLength = 0U;
        #endif
    #else /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */
        #ifdef democonfigCLIENT_USERNAME
            xConnectInfo.pUserName = democonfigCLIENT_USERNAME;
            xConnectInfo.userNameLength = ( uint16_t ) strlen( democonfigCLIENT_USERNAME );
            xConnectInfo.pPassword = democonfigCLIENT_PASSWORD;
            xConnectInfo.passwordLength = ( uint16_t ) strlen( democonfigCLIENT_PASSWORD );
        #endif /* ifdef democonfigCLIENT_USERNAME */
    #endif /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */

    /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
     * is not used in this demo, so it is passed as NULL. */
    xResult = MQTTAgent_Connect( xMQTTContextHandle,
                                 &xConnectInfo,
                                 NULL,
                                 mqttexampleCONNACK_RECV_TIMEOUT_MS,
                                 &xSessionPresent );

    LogInfo( ( "Session present: %d\n", xSessionPresent ) );

    /* Resume a session if desired. */
    if( ( xResult == MQTTSuccess ) && !xCleanSession )
    {
        xResult = MQTTAgent_ResumeSession( xMQTTContextHandle, xSessionPresent );
    }

    return xResult;
}

/*-----------------------------------------------------------*/

static BaseType_t prvSocketConnect( NetworkContext_t * pxNetworkContext )
{
    BaseType_t xConnected = pdFAIL;
    RetryUtilsStatus_t xRetryUtilsStatus = RetryUtilsSuccess;
    RetryUtilsParams_t xReconnectParams;
    const TickType_t transportTimeout = 0;

    #if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 )
        TlsTransportStatus_t xNetworkStatus = TLS_TRANSPORT_CONNECT_FAILURE;
        NetworkCredentials_t xNetworkCredentials = { 0 };

        #ifdef democonfigUSE_AWS_IOT_CORE_BROKER

            /* ALPN protocols must be a NULL-terminated list of strings. Therefore,
             * the first entry will contain the actual ALPN protocol string while the
             * second entry must remain NULL. */
            char * pcAlpnProtocols[] = { NULL, NULL };

            /* The ALPN string changes depending on whether username/password authentication is used. */
            #ifdef democonfigCLIENT_USERNAME
                pcAlpnProtocols[ 0 ] = AWS_IOT_CUSTOM_AUTH_ALPN;
            #else
                pcAlpnProtocols[ 0 ] = AWS_IOT_MQTT_ALPN;
            #endif
            xNetworkCredentials.pAlpnProtos = pcAlpnProtocols;
        #endif /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */

        /* Set the credentials for establishing a TLS connection. */
        xNetworkCredentials.pRootCa = ( const unsigned char * ) democonfigROOT_CA_PEM;
        xNetworkCredentials.rootCaSize = sizeof( democonfigROOT_CA_PEM );
        #ifdef democonfigCLIENT_CERTIFICATE_PEM
            xNetworkCredentials.pClientCert = ( const unsigned char * ) democonfigCLIENT_CERTIFICATE_PEM;
            xNetworkCredentials.clientCertSize = sizeof( democonfigCLIENT_CERTIFICATE_PEM );
            xNetworkCredentials.pPrivateKey = ( const unsigned char * ) democonfigCLIENT_PRIVATE_KEY_PEM;
            xNetworkCredentials.privateKeySize = sizeof( democonfigCLIENT_PRIVATE_KEY_PEM );
        #endif
        xNetworkCredentials.disableSni = democonfigDISABLE_SNI;
    #else /* if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 ) */
        PlaintextTransportStatus_t xNetworkStatus = PLAINTEXT_TRANSPORT_CONNECT_FAILURE;
    #endif /* if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 ) */

    /* We will use a retry mechanism with an exponential backoff mechanism and
     * jitter. We initialize reconnect attempts and interval here. */
    xReconnectParams.maxRetryAttempts = MAX_RETRY_ATTEMPTS;
    RetryUtils_ParamsReset( &xReconnectParams );

    /* Attempt to connect to MQTT broker. If connection fails, retry after a
     * timeout. Timeout value will exponentially increase until the maximum
     * number of attempts are reached.
     */
    do
    {
        /* Establish a TCP connection with the MQTT broker. This example connects to
         * the MQTT broker as specified in democonfigMQTT_BROKER_ENDPOINT and
         * democonfigMQTT_BROKER_PORT at the top of this file. */
        #if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 )
            LogInfo( ( "Creating a TLS connection to %s:%d.",
                       democonfigMQTT_BROKER_ENDPOINT,
                       democonfigMQTT_BROKER_PORT ) );
            xNetworkStatus = TLS_FreeRTOS_Connect( pxNetworkContext,
                                                   democonfigMQTT_BROKER_ENDPOINT,
                                                   democonfigMQTT_BROKER_PORT,
                                                   &xNetworkCredentials,
                                                   mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                                   mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS );
            xConnected = ( xNetworkStatus == TLS_TRANSPORT_SUCCESS ) ? pdPASS : pdFAIL;
        #else /* if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 ) */
            LogInfo( ( "Creating a TCP connection to %s:%d.",
                       democonfigMQTT_BROKER_ENDPOINT,
                       democonfigMQTT_BROKER_PORT ) );
            xNetworkStatus = Plaintext_FreeRTOS_Connect( pxNetworkContext,
                                                         democonfigMQTT_BROKER_ENDPOINT,
                                                         democonfigMQTT_BROKER_PORT,
                                                         mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                                         mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS );
            xConnected = ( xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS ) ? pdPASS : pdFAIL;
        #endif /* if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 ) */

        if( !xConnected )
        {
            LogWarn( ( "Connection to the broker failed. Retrying connection with backoff and jitter." ) );
            xRetryUtilsStatus = RetryUtils_BackoffAndSleep( &xReconnectParams );
        }

        if( xRetryUtilsStatus == RetryUtilsRetriesExhausted )
        {
            LogError( ( "Connection to the broker failed. All attempts exhausted." ) );
        }
    } while( ( xConnected != pdPASS ) && ( xRetryUtilsStatus == RetryUtilsSuccess ) );

    /* Set the socket wakeup callback. */
    if( xConnected )
    {
        ( void ) FreeRTOS_setsockopt( pxNetworkContext->tcpSocket,
                                      0, /* Level - Unused. */
                                      FREERTOS_SO_WAKEUP_CALLBACK,
                                      ( void * ) prvMQTTClientSocketWakeupCallback,
                                      sizeof( &( prvMQTTClientSocketWakeupCallback ) ) );

        ( void ) FreeRTOS_setsockopt( pxNetworkContext->tcpSocket,
                                      0,
                                      FREERTOS_SO_RCVTIMEO,
                                      &transportTimeout,
                                      sizeof( TickType_t ) );

    }

    return xConnected;
}

/*-----------------------------------------------------------*/

static BaseType_t prvSocketDisconnect( NetworkContext_t * pxNetworkContext )
{
    BaseType_t xDisconnected = pdFAIL;

    /* Set the wakeup callback to NULL since the socket will disconnect. */
    ( void ) FreeRTOS_setsockopt( pxNetworkContext->tcpSocket,
                                  0, /* Level - Unused. */
                                  FREERTOS_SO_WAKEUP_CALLBACK,
                                  ( void * ) NULL,
                                  sizeof( void * ) );

    #if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 )
        LogInfo( ( "Disconnecting TLS connection.\n" ) );
        TLS_FreeRTOS_Disconnect( pxNetworkContext );
        xDisconnected = pdPASS;
    #else
        LogInfo( ( "Disconnecting TCP connection.\n" ) );
        PlaintextTransportStatus_t xNetworkStatus = PLAINTEXT_TRANSPORT_CONNECT_FAILURE;
        xNetworkStatus = Plaintext_FreeRTOS_Disconnect( pxNetworkContext );
        xDisconnected = ( xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS ) ? pdPASS : pdFAIL;
    #endif
    return xDisconnected;
}

/*-----------------------------------------------------------*/

static void prvMQTTClientSocketWakeupCallback( Socket_t pxSocket )
{
    BaseType_t xResult;

    /* Just to avoid compiler warnings.  The socket is not used but the function
     * prototype cannot be changed because this is a callback function. */
    ( void ) pxSocket;

    /* A socket used by the MQTT task may need attention.  Send an event
     * to the MQTT task to make sure the task is not blocked on xCommandQueue. */
    if( MQTTAgent_GetNumWaiting() == 0U )
    {
        xResult = MQTTAgent_ProcessLoop( xMQTTContextHandle, NULL, NULL );
//        configASSERT( xResult == pdTRUE );
    }
}

/*-----------------------------------------------------------*/

static void prvCopyPublishToQueue( MQTTPublishInfo_t * pxPublishInfo, /*_RB_ Are these parameters the other way round in the coreMQTT library? */
                                   void * pxResponseQueue )
{
    PublishElement_t xCopiedPublish;

    memset( &xCopiedPublish, 0x00, sizeof( xCopiedPublish ) );
    memcpy( &( xCopiedPublish.xPublishInfo ), pxPublishInfo, sizeof( MQTTPublishInfo_t ) );

    /* Since adding an MQTTPublishInfo_t to a queue will not copy its string buffers,
     * we need to add buffers to a struct and copy the entire structure. We don't
     * need to set xCopiedPublish.xPublishInfo's pointers yet since the actual address
     * will change after the struct is copied into the queue. */
    memcpy( xCopiedPublish.pcTopicNameBuf, pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength );
    memcpy( xCopiedPublish.pcPayloadBuf, pxPublishInfo->pPayload, pxPublishInfo->payloadLength );

    /* Add to response queue. */
    ( void ) xQueueSendToBack( ( QueueHandle_t ) pxResponseQueue, ( void * ) &xCopiedPublish, mqttexampleDEMO_TICKS_TO_WAIT );
}

/*-----------------------------------------------------------*/

extern void vLargeMessageSubscribePublishTask( void * pvParameters );

extern void vSimpleSubscribePublishTask( void * pvParameters );

/*-----------------------------------------------------------*/

static void prvMQTTAgentTask( void * pvParameters )
{
    BaseType_t xNetworkResult = pdFAIL;
    MQTTStatus_t xMQTTStatus = MQTTSuccess;
    MQTTContext_t * pMqttContext = NULL;

    ( void ) pvParameters;

    do
    {
        pMqttContext = MQTTAgent_CommandLoop();

        /* Context is only returned if error occurred. */
        if( pMqttContext != NULL )
        {
            /* Reconnect TCP. */
            xNetworkResult = prvSocketDisconnect( &xNetworkContext );
            configASSERT( xNetworkResult == pdPASS );
            xNetworkResult = prvSocketConnect( &xNetworkContext );
            configASSERT( xNetworkResult == pdPASS );
            /* MQTT Connect with a persistent session. */
            xMQTTStatus = prvMQTTConnect( false );/*_RB_ Should this be true or false? */
        }
    } while( pMqttContext );
}

/*-----------------------------------------------------------*/

static BaseType_t prvCreateMQTTAgent( void )
{
    BaseType_t xResult;

    /* In this demo, send publishes on non-subscribed topics to this queue.
     * Note that this value is not meant to be changed after `MQTTAgent_CommandLoop`
     * has been called, since access to this variable is not protected by thread
     * synchronization primitives. */
    xDefaultResponseQueue = xQueueCreate( 1, sizeof( PublishElement_t ) );

    /* Create the MQTT agent task. This task is only created once and persists
     * across demo iterations. */
    xResult = xTaskCreate(  prvMQTTAgentTask,
                            "MQTTAgent",
                            democonfigDEMO_STACKSIZE,
                            NULL,
                            configMAX_PRIORITIES - 3,
                            &xAgentTask );

    configASSERT( xResult == pdPASS );

    return xResult;
}
/*-----------------------------------------------------------*/

static void prvMQTTDemoTask( void * pvParameters )
{
    BaseType_t xNetworkStatus = pdFAIL;
    MQTTStatus_t xMQTTStatus;
    int32_t i = 0;
    char pcTaskNameBuf[ 10 ];
    extern int vStartOTADemo( MQTTContext_t *pxOTAMQTTConext );

    ( void ) pvParameters;

    ulGlobalEntryTimeMs = prvGetTimeMs();

    /* Connect a TCP socket to the broker. */
    xNetworkStatus = prvSocketConnect( &xNetworkContext );
    configASSERT( xNetworkStatus == pdPASS );

    /* Initialise the MQTT context with the buffer and transport interface. */
    xMQTTStatus = prvMQTTInit();
    configASSERT( xMQTTStatus == MQTTSuccess );

    /* Create the agent task itself. */
    //prvCreateMQTTAgent();

    /* Form an MQTT connection without a persistent session. */
    xMQTTStatus = prvMQTTConnect( true );
    configASSERT( xMQTTStatus == MQTTSuccess );

    xTaskCreate( vLargeMessageSubscribePublishTask, "LargeSubPub", democonfigDEMO_STACKSIZE, NULL, tskIDLE_PRIORITY, NULL );

    /* Create a few instances of vSimpleSubscribePublishTask(). */
    for( i = 0; i < mqttexampleNUM_SUBSCRIBE_PUBLISH_TASKS; i++ )
    {
        memset( pcTaskNameBuf, 0x00, sizeof( pcTaskNameBuf ) );
        snprintf( pcTaskNameBuf, 10, "SubPub%d", i );
        xTaskCreate( vSimpleSubscribePublishTask, pcTaskNameBuf, democonfigDEMO_STACKSIZE, ( void * ) i, tskIDLE_PRIORITY, NULL );
    }

    /* Start the MQTT agent. */
    prvMQTTAgentTask( NULL );
}

/*-----------------------------------------------------------*/

static uint32_t prvGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * mqttexampleMILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

/*-----------------------------------------------------------*/
