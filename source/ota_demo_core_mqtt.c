/*
 * FreeRTOS V202011.00
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
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @file aws_iot_ota_update_demo.c
 * @brief A simple OTA update example.
 *
 * This example initializes the OTA agent to enable OTA updates via the
 * MQTT broker. It simply connects to the MQTT broker with the users
 * credentials and spins in an indefinite loop to allow MQTT messages to be
 * forwarded to the OTA agent for possible processing. The OTA agent does all
 * of the real work; checking to see if the message topic is one destined for
 * the OTA agent. If not, it is simply ignored.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Include common demo header. */
//_RB_#include "aws_demo.h"
#include "demo_config.h" //_RB_ Added.

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* MQTT library includes. */
#include "core_mqtt.h"
#include "freertos_mqtt_agent.h"

/* OTA Library include. */
#include "ota.h"
//_RB_ Order of includes is causing a build error as it tries to define the same thing twice. #include "ota_config.h"
#include "ota_private.h" //_RB_ Looks odd to include a private header file.

/* OTA Library Interface include. */
#include "ota_os_freertos.h"
#include "ota_mqtt_interface.h"
#include "ota_platform_interface.h"

/* Include firmware version struct definition. */
#include "ota_appversion32.h"

/* Include platform abstraction header. */
#include "ota_pal.h"

/*------------- Demo configurations -------------------------*/

/** Note: The device client certificate and private key credentials are
 * obtained by the transport interface implementation (with Secure Sockets)
 * from the demos/include/aws_clientcredential_keys.h file.
 *
 * The following macros SHOULD be defined for this demo which uses both server
 * and client authentications for TLS session:
 *   - keyCLIENT_CERTIFICATE_PEM for client certificate.
 *   - keyCLIENT_PRIVATE_KEY_PEM for client private key.
 */

/**
 * @brief The MQTT broker endpoint used for this demo.
 */
#ifndef democonfigMQTT_BROKER_ENDPOINT
    #define democonfigMQTT_BROKER_ENDPOINT    clientcredentialMQTT_BROKER_ENDPOINT
#endif

/**
 * @brief The root CA certificate belonging to the broker.
 */
#ifndef democonfigROOT_CA_PEM
    #define democonfigROOT_CA_PEM    tlsATS1_ROOT_CERTIFICATE_PEM
#endif

#ifndef democonfigCLIENT_IDENTIFIER

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique so edit as required to ensure no two clients connecting to the
 * same broker use the same client identifier.
 */
    #define democonfigCLIENT_IDENTIFIER    clientcredentialIOT_THING_NAME
#endif

#ifndef democonfigMQTT_BROKER_PORT

/**
 * @brief The port to use for the demo.
 */
    #define democonfigMQTT_BROKER_PORT    clientcredentialMQTT_BROKER_PORT
#endif

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define otaexampleTRANSPORT_SEND_RECV_TIMEOUT_MS    ( 1000U )

/**
 * @brief The maximum number of retries for network operation with server.
 */
#define RETRY_MAX_ATTEMPTS                          ( 5U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define RETRY_MAX_BACKOFF_DELAY_MS                  ( 5000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define RETRY_BACKOFF_BASE_MS                       ( 500U )

/**
 * @brief Size of the network buffer for MQTT packets.
 */
#define otaexampleNETWORK_BUFFER_SIZE               ( 5120U )

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define otaexampleMAX_FILE_PATH_SIZE                ( 260 )

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define otaexampleMAX_STREAM_NAME_SIZE              ( 128 )

/**
 * @brief The delay used in the main OTA Demo task loop to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otaexampleTASK_DELAY_MS                     ( 1000UL )

/**
 * @brief The timeout for waiting for the agent to get suspended after closing the
 * connection.
 */
#define OTA_SUSPEND_TIMEOUT_MS                      ( 5000 )

/**
 * @brief The delay used in the main OTA Demo task loop to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define OTA_EXAMPLE_TASK_DELAY_MS                   ( 500U )

/*
 * @brief Run OTA agent at equal or higher priority as that of demo polling task.
 */
#define OTA_AGENT_TASK_PRIORITY                     ( configMAX_PRIORITIES - 1 )

/**
 * @brief The common prefix for all OTA topics.
 */
#define OTA_TOPIC_PREFIX                            "$aws/things/"

/**
 * @brief The string used for jobs topics.
 */
#define OTA_TOPIC_JOBS                              "jobs"

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define mqttexampleMAX_UINT32                       0xffffffff

/**
 * @brief The string used for streaming service topics.
 */
#define OTA_TOPIC_STREAM                            "streams"
/**
 * @brief The length of #OTA_TOPIC_PREFIX
 */
#define OTA_TOPIC_PREFIX_LENGTH                     ( ( uint16_t ) ( sizeof( OTA_TOPIC_PREFIX ) - 1U ) )

/**
 * @brief Keep alive time reported to the broker while establishing
 * an MQTT connection.
 *
 * @brief The maximum time interval that is permitted to elapse between the point at
 * which the MQTT client finishes transmitting one control Packet and the point it starts
 * sending the next.In the absence of control packet a PINGREQ  is sent. The broker must
 * disconnect a client that does not send a message or a PINGREQ packet in one and a
 * half times the keep alive interval. It is the responsibility of the Client to ensure
 * that the interval between Control Packets being sent does not exceed the this Keep Alive
 * value. In the absence of sending any other Control Packets, the Client MUST send a
 * PINGREQ Packet.
 */
#define otaexampleKEEP_ALIVE_TIMEOUT_SECONDS        ( 60U )

/**
 * @brief Timeout for receiving CONNACK packet in milliseconds.
 */
#define otaexampleCONNACK_RECV_TIMEOUT_MS           ( 1000U )

/**
 * @brief OTA Library task stack size in words.
 */
#define otaexampleSTACK_SIZE                        ( 6000U )

/**
 * @brief Milliseconds per second.
 */
#define MILLISECONDS_PER_SECOND                     ( 1000U )

/**
 * @brief Milliseconds per FreeRTOS tick.
 */
#define MILLISECONDS_PER_TICK                       ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )
/**
 * @brief Ticks to wait till we obtain lock on semaphore.
 */
#define DEFAULT_TICKS_TO_WAIT_FOR_SEMPHR            ( pdMS_TO_TICKS( 2 * otaexampleTASK_DELAY_MS ) )

/**
 * @brief Configure application version.
 */

#define APP_VERSION_MAJOR    0
#define APP_VERSION_MINOR    9
#define APP_VERSION_BUILD    2

/**
 * @brief Update File path buffer.
 */
uint8_t updateFilePath[ otaexampleMAX_FILE_PATH_SIZE ];

/**
 * @brief Certificate File path buffer.
 */
uint8_t certFilePath[ otaexampleMAX_FILE_PATH_SIZE ];

/**
 * @brief Stream name buffer.
 */
uint8_t streamName[ otaexampleMAX_STREAM_NAME_SIZE ];

/**
 * @brief Decode memory.
 */
uint8_t decodeMem[ ( 1U << otaconfigLOG2_FILE_BLOCK_SIZE ) ];

/**
 * @brief Bitmap memory.
 */
uint8_t bitmap[ OTA_MAX_BLOCK_BITMAP_SIZE ];

/**
 * @brief Event buffer.
 */
static OtaEventData_t eventBuffer;

/**
 * @brief Static handle for MQTT context.
 */
static MQTTContext_t xMQTTContext;

/**
 * @brief Static handle for Network context.
 */
NetworkContext_t xNetworkContext;

/**
 * @brief Mutex for synchronizing coreMQTT API calls.
 */
static SemaphoreHandle_t xMqttMutex;

/**
 * @brief Semaphore for synchronizing buffer operations.
 */
static SemaphoreHandle_t xBufferSemaphore;

/**
 * @brief Semaphore for acknowledgment from MQTT packet.
 */
static SemaphoreHandle_t xMqttAckSem;

/**
 * @brief Static buffer used to hold MQTT messages being sent and received.
 */
static uint8_t ucSharedBuffer[ otaexampleNETWORK_BUFFER_SIZE ];

/**
 * @brief Global entry time into the application to use as a reference timestamp
 * in the #prvGetTimeMs function. #prvGetTimeMs will always return the difference
 * between the current time and the global entry time. This will reduce the chances
 * of overflow for the 32 bit unsigned integer used for holding the timestamp.
 */
static uint32_t ulGlobalEntryTimeMs;

static const MQTTContextHandle_t xMQTTContextHandle = 0;

/** @brief Static buffer used to hold MQTT messages being sent and received. */
static MQTTFixedBuffer_t xBuffer =
{
    ucSharedBuffer,
    otaexampleNETWORK_BUFFER_SIZE
};

/**
 * @brief The buffer passed to the OTA Agent from application while initializing.
 */
static OtaAppBuffer_t otaBuffer =
{
    .pUpdateFilePath    = updateFilePath,
    .updateFilePathsize = otaexampleMAX_FILE_PATH_SIZE,
    .pCertFilePath      = certFilePath,
    .certFilePathSize   = otaexampleMAX_FILE_PATH_SIZE,
    .pStreamName        = streamName,
    .streamNameSize     = otaexampleMAX_STREAM_NAME_SIZE,
    .pDecodeMemory      = decodeMem,
    .decodeMemorySize   = ( 1U << otaconfigLOG2_FILE_BLOCK_SIZE ),
    .pFileBitmap        = bitmap,
    .fileBitmapSize     = OTA_MAX_BLOCK_BITMAP_SIZE
};


/**
 * @brief Enum for type of OTA messages received.
 */
typedef enum OtaMessageType
{
    OtaMessageTypeJob = 0,
    OtaMessageTypeStream,
    OtaNumOfMessageType
} OtaMessageType_t;

/**
 * @brief Struct for firmware version.
 */
const AppVersion32_t appFirmwareVersion =
{
    .u.x.major = APP_VERSION_MAJOR,
    .u.x.minor = APP_VERSION_MINOR,
    .u.x.build = APP_VERSION_BUILD,
};

/*-----------------------------------------------------------*/

static OtaMessageType_t getOtaMessageType( const char * pTopicFilter,
                                           uint16_t topicFilterLength )
{
    int retStatus = EXIT_FAILURE;

    uint16_t stringIndex = 0U, fieldLength = 0U, i = 0U;
    OtaMessageType_t retMesageType = OtaNumOfMessageType;

    /* Lookup table for OTA message string. */
    static const char * const pOtaMessageStrings[ OtaNumOfMessageType ] =
    {
        OTA_TOPIC_JOBS,
        OTA_TOPIC_STREAM
    };

    /* Check topic prefix is valid.*/
    if( strncmp( pTopicFilter, OTA_TOPIC_PREFIX, ( size_t ) OTA_TOPIC_PREFIX_LENGTH ) == 0 )
    {
        stringIndex = OTA_TOPIC_PREFIX_LENGTH;

        retStatus = EXIT_SUCCESS;
    }

    /* Check if thing name is valid.*/
    if( retStatus == EXIT_SUCCESS )
    {
        retStatus = EXIT_FAILURE;

        /* Extract the thing name.*/
        for( ; stringIndex < topicFilterLength; stringIndex++ )
        {
            if( pTopicFilter[ stringIndex ] == ( char ) '/' )
            {
                break;
            }
            else
            {
                fieldLength++;
            }
        }

        if( fieldLength > 0 )
        {
            /* Check thing name.*/
            if( strncmp( &pTopicFilter[ stringIndex - fieldLength ],
                         democonfigCLIENT_IDENTIFIER,
                         ( size_t ) ( fieldLength ) ) == 0 )
            {
                stringIndex++;

                retStatus = EXIT_SUCCESS;
            }
        }
    }

    /* Check the message type from topic.*/
    if( retStatus == EXIT_SUCCESS )
    {
        fieldLength = 0;

        /* Extract the topic type.*/
        for( ; stringIndex < topicFilterLength; stringIndex++ )
        {
            if( pTopicFilter[ stringIndex ] == ( char ) '/' )
            {
                break;
            }
            else
            {
                fieldLength++;
            }
        }

        if( fieldLength > 0 )
        {
            for( i = 0; i < OtaNumOfMessageType; i++ )
            {
                /* check thing name.*/
                if( strncmp( &pTopicFilter[ stringIndex - fieldLength ],
                             pOtaMessageStrings[ i ],
                             ( size_t ) ( fieldLength ) ) == 0 )
                {
                    break;
                }
            }

            if( i < OtaNumOfMessageType )
            {
                retMesageType = i;
            }
        }
    }

    return retMesageType;
}

/*-----------------------------------------------------------*/

/**
 * @brief Connect to MQTT broker with reconnection retries.
 *
 * If connection fails, retry is attempted after a timeout.
 * Timeout value will exponentially increase until maximum
 * timeout value is reached or the number of attempts are exhausted.
 *
 * @param[out] pxNetworkContext The output parameter to return the created network context.
 *
 * @return pdFAIL on failure; pdPASS on successful TLS+TCP network connection.
 */
static BaseType_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pNetworkContext );

/**
 * @brief The application callback function for getting the incoming publishes,
 * incoming acks, and ping responses reported from the MQTT library.
 *
 * @param[in] pxMQTTContext MQTT context pointer.
 * @param[in] pxPacketInfo Packet Info pointer for the incoming packet.
 * @param[in] pxDeserializedInfo Deserialized information from the incoming packet.
 */
static void prvEventCallback( MQTTContext_t * pxMQTTContext,
                              MQTTPacketInfo_t * pxPacketInfo,
                              MQTTDeserializedInfo_t * pxDeserializedInfo );

/*
 * Publish a message to the specified client/topic at the given QOS.
 */
static OtaErr_t mqttPublish( const char * const pacTopic,
                             uint16_t topicLen,
                             const char * pMsg,
                             uint32_t msgSize,
                             uint8_t qos );

/*
 * Subscribe to the topics.
 */
static OtaErr_t mqttSubscribe( const char * pTopicFilter,
                               uint16_t topicFilterLength,
                               uint8_t qos );

/*
 * Unsubscribe from the topics.
 */
static OtaErr_t mqttUnsubscribe( const char * pTopicFilter,
                                 uint16_t topicFilterLength,
                                 uint8_t qos );

/*-----------------------------------------------------------*/

static void prvEventCallback( MQTTContext_t * pxMQTTContext,
                              MQTTPacketInfo_t * pxPacketInfo,
                              MQTTDeserializedInfo_t * pxDeserializedInfo )
{
    configASSERT( pxMQTTContext != NULL );
    configASSERT( pxPacketInfo != NULL );
    configASSERT( pxDeserializedInfo != NULL );

    /* Handle incoming publish. The lower 4 bits of the publish packet
     * type is used for the dup, QoS, and retain flags. Hence masking
     * out the lower bits to check if the packet is publish. */
    if( ( pxPacketInfo->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
    {
        configASSERT( pxDeserializedInfo->pPublishInfo != NULL );
        /* Handle incoming publish. */
        SubscriptionManager_DispatchHandler( pxMQTTContext, pxDeserializedInfo->pPublishInfo );
    }
    else
    {
        /* Handle other packets. */
        switch( pxPacketInfo->type )
        {
            case MQTT_PACKET_TYPE_SUBACK:
                LogInfo( ( "Received SUBACK.\n\n" ) );
                xSemaphoreGive( xMqttAckSem );
                break;

            case MQTT_PACKET_TYPE_UNSUBACK:
                LogInfo( ( "Received UNSUBACK.\n\n" ) );
                xSemaphoreGive( xMqttAckSem );
                break;

            case MQTT_PACKET_TYPE_PINGRESP:

                /* Nothing to be done from application as library handles
                 * PINGRESP. */
                LogWarn( ( "PINGRESP should not be handled by the application "
                           "callback when using MQTT_ProcessLoop.\n\n" ) );
                break;

            case MQTT_PACKET_TYPE_PUBACK:
                LogInfo( ( "PUBACK received for packet id %u.\n\n",
                           pxDeserializedInfo->packetIdentifier ) );
                xSemaphoreGive( xMqttAckSem );
                break;

            /* Any other packet type is invalid. */
            default:
                LogError( ( "Unknown packet type received:(%02x).\n\n",
                            pxPacketInfo->type ) );
        }
    }
}

/*-----------------------------------------------------------*/

static uint32_t prvGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

static int32_t prvGenerateRandomNumber()
{
    uint32_t ulRandomNum;
#ifdef _RB_
    /* Use the PKCS11 module to generate a random number. */
    if( xPkcs11GenerateRandomNumber( ( uint8_t * ) &ulRandomNum,
                                     ( sizeof( ulRandomNum ) ) ) == pdPASS )
    {
        ulRandomNum = ( ulRandomNum & INT32_MAX );
    }
    else
    {
        /* Set the return value as negative to indicate failure. */
        ulRandomNum = -1;
    }
#endif
    ulRandomNum = rand();
    return ( int32_t ) ulRandomNum;
}

/*-----------------------------------------------------------*/

static BaseType_t prvCreateMQTTConnectionWithBroker( MQTTContext_t * pxMQTTContext,
                                                     NetworkContext_t * pxNetworkContext )
{
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent;
    TransportInterface_t xTransport;
    BaseType_t xStatus = pdFAIL;

    /* Fill in Transport Interface send and receive function pointers. */
    xTransport.pNetworkContext = pxNetworkContext;
    xTransport.send = SecureSocketsTransport_Send;
    xTransport.recv = SecureSocketsTransport_Recv;

    /* Initialize MQTT library. */
    xResult = MQTT_Init( pxMQTTContext, &xTransport, prvGetTimeMs, prvEventCallback, &xBuffer );
    configASSERT( xResult == MQTTSuccess );

    /* Some fields are not used in this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = true;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = democonfigCLIENT_IDENTIFIER;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( democonfigCLIENT_IDENTIFIER );

    /* Set MQTT keep-alive period. If the application does not send packets at an interval less than
     * the keep-alive period, the MQTT library will send PINGREQ packets. */
    xConnectInfo.keepAliveSeconds = otaexampleKEEP_ALIVE_TIMEOUT_SECONDS;

    if( xSemaphoreTake( xMqttMutex, DEFAULT_TICKS_TO_WAIT_FOR_SEMPHR ) == pdTRUE )
    {
        /* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it
         * is passed as NULL. */
        xResult = MQTT_Connect( pxMQTTContext,
                                &xConnectInfo,
                                NULL,
                                otaexampleCONNACK_RECV_TIMEOUT_MS,
                                &xSessionPresent );
        xSemaphoreGive( xMqttMutex );
    }
    else
    {
        LogError( ( "Failed to acquire mqtt mutex for executing MQTT_Connect." ) );
    }

    if( xResult != MQTTSuccess )
    {
        LogError( ( "Failed to establish MQTT connection: Server=%s, MQTTStatus=%s",
                    democonfigMQTT_BROKER_ENDPOINT, MQTT_Status_strerror( xResult ) ) );
    }
    else
    {
        /* Successfully established and MQTT connection with the broker. */
        LogInfo( ( "An MQTT connection is established with %s.", democonfigMQTT_BROKER_ENDPOINT ) );
        xStatus = pdPASS;
    }

    return xStatus;
}

static BaseType_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pxNetworkContext )
{
    ServerInfo_t xServerInfo = { 0 };
    SocketsConfig_t xSocketsConfig = { 0 };
    BaseType_t xStatus = pdPASS;
    TransportSocketStatus_t xNetworkStatus = TRANSPORT_SOCKET_STATUS_SUCCESS;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xReconnectParams;
    uint16_t usNextRetryBackOff = 0U;

    /* Set the credentials for establishing a TLS connection. */
    /* Initializer server information. */
    xServerInfo.pHostName = democonfigMQTT_BROKER_ENDPOINT;
    xServerInfo.hostNameLength = strlen( democonfigMQTT_BROKER_ENDPOINT );
    xServerInfo.port = democonfigMQTT_BROKER_PORT;

    /* Configure credentials for TLS mutual authenticated session. */
    xSocketsConfig.enableTls = true;
    xSocketsConfig.pAlpnProtos = NULL;
    xSocketsConfig.maxFragmentLength = 0;
    xSocketsConfig.disableSni = false;
    xSocketsConfig.pRootCa = democonfigROOT_CA_PEM;
    xSocketsConfig.rootCaSize = sizeof( democonfigROOT_CA_PEM );
    xSocketsConfig.sendTimeoutMs = otaexampleTRANSPORT_SEND_RECV_TIMEOUT_MS;
    xSocketsConfig.recvTimeoutMs = otaexampleTRANSPORT_SEND_RECV_TIMEOUT_MS;

    /* Initialize reconnect attempts and interval. */
    BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                       RETRY_BACKOFF_BASE_MS,
                                       RETRY_MAX_BACKOFF_DELAY_MS,
                                       RETRY_MAX_ATTEMPTS,
                                       prvGenerateRandomNumber );

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase till maximum
     * attempts are reached.
     */
    do
    {
        /* Establish a TLS session with the MQTT broker. This example connects to
         * the MQTT broker as specified in democonfigMQTT_BROKER_ENDPOINT and
         * democonfigMQTT_BROKER_PORT at the top of this file. */
        LogInfo( ( "Creating a TLS connection to %s:%u.",
                   democonfigMQTT_BROKER_ENDPOINT,
                   democonfigMQTT_BROKER_PORT ) );
        /* Attempt to create a mutually authenticated TLS connection. */
        xNetworkStatus = SecureSocketsTransport_Connect( pxNetworkContext,
                                                         &xServerInfo,
                                                         &xSocketsConfig );

        if( xNetworkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS )
        {
            /* Get back-off value (in milliseconds) for the next connection retry. */
            xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xReconnectParams, &usNextRetryBackOff );
            configASSERT( xBackoffAlgStatus != BackoffAlgorithmRngFailure );

            if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                LogError( ( "Connection to the broker failed, all attempts exhausted." ) );
                xStatus = pdFAIL;
            }
            else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogWarn( ( "Connection to the broker failed. Retrying connection after backoff delay." ) );
                vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );

                LogInfo( ( "Retry attempt %lu out of maximum retry attempts %lu.",
                           ( xReconnectParams.attemptsDone + 1 ),
                           xReconnectParams.maxRetryAttempts ) );
            }
        }
    } while( ( xNetworkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );

    return xStatus;
}

/*-----------------------------------------------------------*/

/**
 * @brief The OTA agent has completed the update job or it is in
 * self test mode. If it was accepted, we want to activate the new image.
 * This typically means we should reset the device to run the new firmware.
 * If now is not a good time to reset the device, it may be activated later
 * by your user code. If the update was rejected, just return without doing
 * anything and we will wait for another job. If it reported that we should
 * start test mode, normally we would perform some kind of system checks to
 * make sure our new firmware does the basic things we think it should do
 * but we will just go ahead and set the image as accepted for demo purposes.
 * The accept function varies depending on your platform. Refer to the OTA
 * PAL implementation for your platform in aws_ota_pal.c to see what it
 * does for you.
 *
 * @param[in] event Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @return None.
 */
static void otaAppCallback( OtaJobEvent_t event )
{
    OtaErr_t err = OtaErrUninitialized;

    /* OTA job is completed. so delete the MQTT and network connection. */
    if( event == OtaJobEventActivate )
    {
        LogInfo( ( "Received OtaJobEventActivate callback from OTA Agent." ) );

        /* OTA job is completed. so delete the network connection. */
        /*MQTT_Disconnect( &mqttContext ); */

        /* Activate the new firmware image. */
        OTA_ActivateNewImage();

        /* We should never get here as new image activation must reset the device.*/
        LogError( ( "New image activation failed." ) );

        for( ; ; )
        {
        }
    }
    else if( event == OtaJobEventFail )
    {
        LogInfo( ( "Received OtaJobEventFail callback from OTA Agent." ) );

        /* Nothing special to do. The OTA agent handles it. */
    }
    else if( event == OtaJobEventStartTest )
    {
        /* This demo just accepts the image since it was a good OTA update and networking
         * and services are all working (or we would not have made it this far). If this
         * were some custom device that wants to test other things before calling it OK,
         * this would be the place to kick off those tests before calling OTA_SetImageState()
         * with the final result of either accepted or rejected. */

        LogInfo( ( "Received OtaJobEventStartTest callback from OTA Agent." ) );
        err = OTA_SetImageState( OtaImageStateAccepted );

        if( err != OtaErrNone )
        {
            LogError( ( " Error! Failed to set image state as accepted." ) );
        }
    }
}

/*-----------------------------------------------------------*/

static void mqttDataCallback( MQTTPublishInfo_t * pPublishInfo,
                              MQTTContext_t * pContext )
{
    configASSERT( pPublishInfo != NULL );
    configASSERT( pContext != NULL );

    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    LogInfo( ( "Received data message callback, size %d.\n\n", pPublishInfo->payloadLength ) );

    pData = &eventBuffer;

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedFileBlock;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );/*_RB_ Is this signaling itself?  Could deadlock. */
    }
    else
    {
        LogError( ( "Error: No OTA data buffers available.\r\n" ) );
    }
}

/*-----------------------------------------------------------*/

static void mqttJobCallback( MQTTContext_t * pContext,
                             MQTTPublishInfo_t * pPublishInfo )
{
    configASSERT( pPublishInfo != NULL );
    configASSERT( pContext != NULL );

    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    LogInfo( ( "Received job message callback, size %d.\n\n", pPublishInfo->payloadLength ) );

    pData = &eventBuffer; /*_RB_ Is access to this thread safe? */

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );//_RB_ Is there a buffer overflow check before this?
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedJobDocument;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );
    }
    else
    {
        LogError( ( "Error: No OTA data buffers available.\r\n" ) );
    }
}
/*-----------------------------------------------------------*/
static SubscriptionManagerCallback_t otaMessageCallback[ OtaNumOfMessageType ] = { mqttJobCallback, mqttDataCallback };
/*-----------------------------------------------------------*/

static void prvCommandCallback( TaskHandle_t xTaskToNotify,
                                         MQTTStatus_t xReturnStatus )
{
    configASSERT( xTaskToNotify );
    xTaskNotify( xTaskToNotify, xReturnStatus, eSetValueWithOverwrite );
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvSubscribeToTopic( MQTTQoS_t xQoS,
                                       char * pcTopicFilter,
                                       void * pCallback )
{
    BaseType_t xCommandAdded;
    MQTTStatus_t xReturn;
    uint32_t ulNotifiedValue;
    MQTTSubscribeInfo_t xSubscribeInfo [ 100 ];
    int iNext = 0;
    TaskHandle_t xTaskHandle = xTaskGetCurrentTaskHandle();

    xSubscribeInfo[ iNext ].pTopicFilter = pcTopicFilter;
    xSubscribeInfo[ iNext ].topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );
    xSubscribeInfo[ iNext ].qos = xQoS;

    LogInfo( ( "Subscribing to topic filter: %s", pcTopicFilter ) );
    xTaskNotifyStateClear( NULL );

    xCommandAdded = MQTTAgent_Subscribe( xMQTTContextHandle,
                                         &( xSubscribeInfo[ iNext ] ),
                                         ( void * ) pCallback,
                                         NULL,
                                         prvCommandCallback,
                                         ( void * ) xTaskHandle );

    configASSERT( xCommandAdded == true );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( xCommandAdded != pdFALSE )
    {
        xCommandAdded = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, &ulNotifiedValue, pdMS_TO_TICKS( otaexampleTASK_DELAY_MS ) );
        configASSERT( xCommandAdded );
    }

    if( xCommandAdded != pdFALSE )
    {
        xReturn = MQTTSuccess;
        iNext++;
    }
    else
    {
        xReturn = MQTTSendFailed;
    }

    configASSERT( xReturn == MQTTSuccess );

    return xReturn;
}



static OtaErr_t mqttSubscribe( const char * pTopicFilter,
                               uint16_t topicFilterLength,
                               uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    SubscriptionManagerStatus_t subscriptionStatus = SUBSCRIPTION_MANAGER_SUCCESS;
    OtaMessageType_t otaMessageType;

    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTContext_t * pMqttContext = &xMQTTContext;
    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];

    configASSERT( pMqttContext != NULL );
    configASSERT( pTopicFilter != NULL );
    configASSERT( topicFilterLength > 0 );

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    /* This example subscribes to only one topic and uses QOS1. */
    pSubscriptionList[ 0 ].qos = qos;
    pSubscriptionList[ 0 ].pTopicFilter = pTopicFilter;
    pSubscriptionList[ 0 ].topicFilterLength = topicFilterLength;

    if( xSemaphoreTake( xMqttMutex, DEFAULT_TICKS_TO_WAIT_FOR_SEMPHR ) == pdTRUE )
    {
        /* Send SUBSCRIBE packet. */
        mqttStatus = MQTT_Subscribe( pMqttContext,
                                     pSubscriptionList,
                                     sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                                     MQTT_GetPacketId( pMqttContext ) );
        xSemaphoreGive( xMqttMutex );
    }
    else
    {
        LogError( ( "Failed to acquire mqtt mutex for executing MQTT_Subscribe" ) );
    }

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send SUBSCRIBE packet to broker with error = %u.",
                    mqttStatus ) );

        otaRet = OtaMqttSubscribeFailed;
    }
    else
    {
        if( xSemaphoreTake( xMqttAckSem, DEFAULT_TICKS_TO_WAIT_FOR_SEMPHR ) == pdTRUE )
        {
            LogInfo( ( "SUBSCRIBED to topic %.*s to broker.\n\n",
                   topicFilterLength,
                   pTopicFilter ) );
        }
        else
        {
            LogError( ( "Failed to receive suback for topic %.*s.\n\n",
                        topicFilterLength,
                        pTopicFilter ) );
            otaRet = OtaMqttSubscribeFailed;
        }
    }

    if( otaRet == OtaMqttSuccess )
    {
        otaMessageType = getOtaMessageType( pTopicFilter, topicFilterLength );

        assert( ( otaMessageType >= 0 ) && ( otaMessageType < OtaNumOfMessageType ) );

        /* Register callback to subscription manager. */
        subscriptionStatus = SubscriptionManager_RegisterCallback( pTopicFilter, topicFilterLength, otaMessageCallback[ otaMessageType ] );

        if( subscriptionStatus != SUBSCRIPTION_MANAGER_SUCCESS )
        {
            LogWarn( ( "Failed to register a callback to subscription manager with error = %d.",
                       subscriptionStatus ) );
        }
    }

    return otaRet;
}

/*
 * Publish a message to the specified client/topic at the given QOS.
 */
static OtaErr_t mqttPublish( const char * const pacTopic,
                             uint16_t topicLen,
                             const char * pMsg,
                             uint32_t msgSize,
                             uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;

    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTPublishInfo_t publishInfo;
    MQTTContext_t * pMqttContext = pxMQTTContext;
    BaseType_t xCommandAdded;
    TaskHandle_t xTaskHandle;
    uint32_t ulNotifiedValue;

    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = qos;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    xTaskHandle = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear( NULL );

    xCommandAdded = MQTTAgent_Publish( xMQTTContextHandle, &publishInfo, prvCommandCallback, ( void * ) xTaskHandle );

    configASSERT( xCommandAdded == pdTRUE );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( qos != MQTTQoS0 )
    {
        if( xCommandAdded != pdFALSE )
        {
            xCommandAdded = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, &ulNotifiedValue, pdMS_TO_TICKS( otaexampleTASK_DELAY_MS ) );
            configASSERT( xCommandAdded );
        }
    }

    if( xCommandAdded == pdTRUE )
    {
        mqttStatus = MQTTSuccess;
    }
    else
    {
        mqttStatus = MQTTNoDataAvailable;
    }

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send PUBLISH packet to broker with error = %u.", mqttStatus ) );

        otaRet = OTA_ERR_PUBLISH_FAILED;
    }
    else
    {
        LogInfo( ( "Sent PUBLISH packet to broker %.*s to broker.\n\n",
                   topicLen,
                   pacTopic ) );

        otaRet = OtaErrNone;
    }

    return otaRet;
}

static OtaErr_t mqttUnsubscribe( const char * pTopicFilter,
                                 uint16_t topicFilterLength,
                                 uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    MQTTStatus_t mqttStatus = MQTTBadParameter;

    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];
    MQTTContext_t * pMqttContext = pxMQTTContext;

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    /* This example subscribes to and unsubscribes from only one topic
     * and uses QOS1. */
    pSubscriptionList[ 0 ].qos = qos;
    pSubscriptionList[ 0 ].pTopicFilter = pTopicFilter;
    pSubscriptionList[ 0 ].topicFilterLength = topicFilterLength;

    if( xSemaphoreTake( xMqttMutex, DEFAULT_TICKS_TO_WAIT_FOR_SEMPHR ) == pdTRUE )
    {
    /* Send UNSUBSCRIBE packet. */
    mqttStatus = MQTT_Unsubscribe( pMqttContext,
                                   pSubscriptionList,
                                   sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                                   MQTT_GetPacketId( pMqttContext ) );

        xSemaphoreGive( xMqttMutex );
    }
    else
    {
        LogError( ( "Failed to acquire mutex for executing MQTT_Unsubscribe" ) );
    }
    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send SUBSCRIBE packet to broker with error = %u.",
                    mqttStatus ) );

        otaRet = OtaMqttUnsubscribeFailed;
    }
    else
    {
        if( xSemaphoreTake( xMqttAckSem, DEFAULT_TICKS_TO_WAIT_FOR_SEMPHR ) == pdTRUE )
        {
        LogInfo( ( "SUBSCRIBE topic %.*s to broker.\n\n",
                   topicFilterLength,
                   pTopicFilter ) );
        }
        else
        {
            LogError( ( "Failed to get unsuback for topic %.*s.\n\n",
                        topicFilterLength,
                        pTopicFilter ) );
            otaRet = OtaMqttUnsubscribeFailed;
        }
    }

    return otaRet;
}

/*-----------------------------------------------------------*/

static void setOtaInterfaces( OtaInterfaces_t * pOtaInterfaces )
{
    configASSERT( pOtaInterfaces != NULL );

    /* Initialize OTA library OS Interface. */
    pOtaInterfaces->os.event.init = OtaInitEvent_FreeRTOS;
    pOtaInterfaces->os.event.send = OtaSendEvent_FreeRTOS;
    pOtaInterfaces->os.event.recv = OtaReceiveEvent_FreeRTOS;
    pOtaInterfaces->os.event.deinit = OtaDeinitEvent_FreeRTOS;
    pOtaInterfaces->os.timer.start = OtaStartTimer_FreeRTOS;
    pOtaInterfaces->os.timer.stop = OtaStopTimer_FreeRTOS;
    pOtaInterfaces->os.timer.delete = OtaDeleteTimer_FreeRTOS;
    pOtaInterfaces->os.mem.malloc = Malloc_FreeRTOS;
    pOtaInterfaces->os.mem.free = Free_FreeRTOS;

    /* Initialize the OTA library MQTT Interface.*/
    pOtaInterfaces->mqtt.subscribe = mqttSubscribe;
    pOtaInterfaces->mqtt.publish = mqttPublish;
    pOtaInterfaces->mqtt.unsubscribe = mqttUnsubscribe;

    /* Initialize the OTA library PAL Interface.*/
    pOtaInterfaces->pal.getPlatformImageState = otaPal_GetPlatformImageState;
    pOtaInterfaces->pal.setPlatformImageState = otaPal_SetPlatformImageState;
    pOtaInterfaces->pal.writeBlock = otaPal_WriteBlock;
    pOtaInterfaces->pal.activate = otaPal_ActivateNewImage;
    pOtaInterfaces->pal.closeFile = otaPal_CloseFile;
    pOtaInterfaces->pal.reset = otaPal_ResetDevice;
    pOtaInterfaces->pal.abort = otaPal_Abort;
    pOtaInterfaces->pal.createFile = otaPal_CreateFileForRx;
}

static BaseType_t prvEstablishConnection( void )
{
    BaseType_t xRet = pdFAIL;

    /* Attempt to establish TLS session with MQTT broker. If connection fails,
     * retry after a timeout. Timeout value will be exponentially increased until
     * the maximum number of attempts are reached or the maximum timeout value is reached.
     * The function returns a failure status if the TLS over TCP connection cannot be established
     * to the broker after the configured number of attempts. */
    xRet = prvConnectToServerWithBackoffRetries( &xNetworkContext );

    if( xRet == pdPASS )
    {
        /* Sends an MQTT Connect packet over the already established TLS connection,
         * and waits for connection acknowledgment (CONNACK) packet. */
        LogInfo( ( "Creating an MQTT connection to %s.", democonfigMQTT_BROKER_ENDPOINT ) );
        xRet = prvCreateMQTTConnectionWithBroker( &xMQTTContext, &xNetworkContext );
    }

    return xRet;
}
static TransportSocketStatus_t prvDisconnect( void )
{
    /* Transport socket return status. */
    TransportSocketStatus_t xNetworkStatus;

    /* Disconnect from broker. */
    LogInfo( ( "Disconnecting the MQTT connection with %s.", democonfigMQTT_BROKER_ENDPOINT ) );
    MQTT_Disconnect( &xMQTTContext );

    /* Close the network connection. */
    xNetworkStatus = SecureSocketsTransport_Disconnect( &xNetworkContext );

    if( xNetworkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS )
    {
        LogError( ( "SecureSocketsTransport_Disconnect() failed to close the network connection. "
                    "StatusCode=%d.", ( int ) xNetworkStatus ) );
    }

    return xNetworkStatus;
}
/*-----------------------------------------------------------*/

int vStartOTADemo( MQTTContext_t *pxOTAMQTTConext )
{
    /* Status indicating a successful demo or not. */
    int32_t returnStatus = EXIT_SUCCESS;

    /* FreeRTOS APIs return status. */
    BaseType_t xRet = pdFAIL;

    /* coreMQTT library return status. */
    MQTTStatus_t mqttStatus = MQTTSuccess;

    /* OTA library return status. */
    OtaErr_t otaRet = OtaErrNone;

    /* OTA Agent state returned from calling OTA_GetAgentState.*/
    OtaState_t state = OtaAgentStateStopped;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t eventMsg = { 0 };

    /* OTA Agent thread handle.*/
    TaskHandle_t xOtaTaskHandle = NULL;

    /* OTA library packet statistics per job.*/
    OtaAgentStatistics_t otaStatistics = { 0 };
    /* OTA interface context required for library interface functions.*/
    OtaInterfaces_t otaInterfaces;

    BaseType_t xIsConnectionEstablished = pdFALSE;

    pxMQTTContext = pxOTAMQTTConext;
    /* Maximum time to wait for the OTA agent to get suspended. */
    TickType_t xSuspendTimeout;

    /* Set OTA Library interfaces.*/
    setOtaInterfaces( &otaInterfaces );

    LogInfo( ( "OTA over MQTT demo, Application version %u.%u.%u",
               appFirmwareVersion.u.x.major,
               appFirmwareVersion.u.x.minor,
               appFirmwareVersion.u.x.build ) );
    /****************************** Init OTA Library. ******************************/

    if( ( otaRet = OTA_Init( &otaBuffer,
                                  &otaInterfaces,
                                  ( const uint8_t * ) ( democonfigCLIENT_IDENTIFIER ),
                             otaAppCallback ) ) != OtaErrNone )
    {
        LogError( ( "Failed to initialize OTA Agent, exiting = %u.",
                    otaRet ) );

        returnStatus = EXIT_FAILURE;
    }

    /****************************** Create OTA Task. ******************************/

    if( otaRet == OtaErrNone )
    {
        if( ( xRet = xTaskCreate( otaAgentTask,
                                  "OTA Agent Task",
                                  otaexampleSTACK_SIZE,
                                  NULL,
                                  OTA_AGENT_TASK_PRIORITY,
                                  &xOtaTaskHandle ) ) != pdPASS )
        {
            LogError( ( "Failed to start OTA task: "
                        ",errno=%d",
                        xRet ) );

            returnStatus = EXIT_FAILURE;
        }
    }

    /***************************Start OTA demo loop. ******************************/

    if( xRet == pdPASS )
    {
        /*
         * Wait forever for OTA traffic but allow other tasks to run and output
         * statistics only once per second. */
        while( ( ( state = OTA_GetState() ) != OtaAgentStateStopped ) )
        {
            if( xIsConnectionEstablished != pdTRUE )
            {
                //_RB_xRet = prvEstablishConnection();

                if( xRet == pdPASS )
                {
                    xIsConnectionEstablished = pdTRUE;

                    if( state == OtaAgentStateSuspended )
                    {
                        /* Resume OTA operations. */
                        OTA_Resume();
                    }
                    else
                    {
                        /* Send start event to OTA Agent.*/
                        eventMsg.eventId = OtaAgentEventStart;
                        OTA_SignalEvent( &eventMsg );
vTaskSuspend( NULL ); /*_RB*/
                    }
                }
            }

            if( xIsConnectionEstablished == pdTRUE )
            {
                /* Loop to receive packet from transport interface. */
//_RB_                mqttStatus = MQTT_ProcessLoop( &xMQTTContext, otaexampleTASK_DELAY_MS );

                if( mqttStatus == MQTTSuccess )
                {
                    /* Get OTA statistics for currently executing job. */
                    OTA_GetStatistics( &otaStatistics );
                    LogInfo( ( " Received: %u   Queued: %u   Processed: %u   Dropped: %u",
                               otaStatistics.otaPacketsReceived,
                               otaStatistics.otaPacketsQueued,
                               otaStatistics.otaPacketsProcessed,
                               otaStatistics.otaPacketsDropped ) );
                }
                else
                {
                    LogError( ( "MQTT_ProcessLoop returned with status = %s.",
                                MQTT_Status_strerror( mqttStatus ) ) );

                    /* Disconnect from broker and close connection. */
//_RB_                    prvDisconnect();

                    xIsConnectionEstablished = pdFALSE;

                    /* Suspend OTA operations. */
                    otaRet = OTA_Suspend();

                    if( otaRet != OtaErrNone )
                    {
                        LogError( ( "OTA failed to suspend. "
                                    "StatusCode=%d.", otaRet ) );
                    }
                }
            }
        }

        if( xOtaTaskHandle != NULL )
        {
            vTaskDelete( xOtaTaskHandle );
            returnStatus = EXIT_SUCCESS;
        }
    }

    return returnStatus;
}

/**
 * @brief The function that runs the OTA demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunOtaCoreMqttDemo( bool awsIotMqttMode,
                        const char * pIdentifier,
                        void * pNetworkServerInfo,
                        void * pNetworkCredentialInfo,
                        const IotNetworkInterface_t * pNetworkInterface )
{
    /* Remove compiler warnings about unused parameters. */
    ( void ) awsIotMqttMode;
    ( void ) pIdentifier;
    ( void ) pNetworkServerInfo;
    ( void ) pNetworkCredentialInfo;
    ( void ) pNetworkInterface;

    /* Return error status. */
    int32_t returnStatus = EXIT_SUCCESS;

    /* Semaphore initialization flag. */
    bool xBufferSemInitialized = false;
    bool xMqttMutexInitialized = false;
    bool xMqttAckSemInitialized = false;

    /* Initialize semaphore for buffer operations. */
    xBufferSemaphore = xSemaphoreCreateBinary();

    if( xBufferSemaphore == NULL )
    {
        LogError( ( "Failed to initialize buffer semaphore." ) );

        returnStatus = EXIT_FAILURE;
    }
    else
    {
        xBufferSemInitialized = true;
    }

    /* Initialize mutex for coreMQTT APIs. */
    xMqttMutex = xSemaphoreCreateMutex();

    if( xMqttMutex == NULL )
    {
        LogError( ( "Failed to initialize mutex for mqtt apis" ) );

        returnStatus = EXIT_FAILURE;
    }
    else
    {
        xMqttMutexInitialized = true;
    }

    /* Initialize mutex for coreMQTT APIs. */
    xMqttAckSem = xSemaphoreCreateBinary();

    if( xMqttAckSem == NULL )
    {
        LogError( ( "Failed to initialize semaphore for MQTT acknowledgments." ) );

        returnStatus = EXIT_FAILURE;
    }
    else
    {
        xMqttAckSemInitialized = true;
    }

    /*TODO: initialize mqtt. */
    /************************* Start OTA demo. ****************************/

    if( returnStatus == EXIT_SUCCESS )
    {
        /* Start OTA demo. */
        returnStatus = prvStartOTADemo();
    }

    if( xBufferSemInitialized == true )
    {
        /* Cleanup semaphore created for buffer operations. */
        vSemaphoreDelete( xBufferSemaphore );
    }

    if( xMqttMutexInitialized == true )
    {
        /* Cleanup mutex created for MQTT operations. */
        vSemaphoreDelete( xMqttMutex );
    }

    if( xMqttAckSemInitialized == true )
    {
        vSemaphoreDelete( xMqttAckSem );
    }

    return returnStatus;
}
