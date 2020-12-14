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
 * @file ota_demo_core_mqtt.c
 * @brief OTA Update example.
 *
 * The example shows how to perform OTA update using OTA agent and coreMQTT
 * library. The example creates the OTA agent task and then spins in its own task
 * publishing OTA statistics periodically within a configured interval.
 * The OTA agent MQTT handlers are implemented using MQTT agent APIs, which
 * allows OTA application to be run concurrently with other MQTT application
 * tasks.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ota_config.h"
#include "demo_config.h"

/* MQTT library includes. */
#include "freertos_mqtt_agent.h"

/* OTA Library include. */
#include "ota.h"

/* OTA Library Interface include. */
#include "ota_os_freertos.h"
#include "ota_mqtt_interface.h"
#include "ota_platform_interface.h"

/* Include firmware version struct definition. */
#include "ota_appversion32.h"

/* Include platform abstraction header. */
#include "ota_pal.h"

/*------------- Demo configurations -------------------------*/

#ifndef democonfigCLIENT_IDENTIFIER

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique so edit as required to ensure no two clients connecting to the
 * same broker use the same client identifier.
 */
    #define democonfigCLIENT_IDENTIFIER    clientcredentialIOT_THING_NAME
#endif

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define otaexampleMAX_FILE_PATH_SIZE            ( 260 )

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define otaexampleMAX_STREAM_NAME_SIZE          ( 128 )

/**
 * @brief The delay used in the main OTA Demo task loop to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otaexampleTASK_DELAY_MS                 ( 1000UL )

/**
 * @brief The delay used in the main OTA Demo task loop to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otaexampleMQTT_DELAY_MS                 ( 5000UL )

/*
 * @brief Run OTA agent at equal or higher priority as that of demo polling task.
 */
#define OTA_AGENT_TASK_PRIORITY                 ( tskIDLE_PRIORITY )

/**
 * @brief The common prefix for all OTA topics.
 */
#define OTA_TOPIC_PREFIX                        "$aws/things/"

/**
 * @brief The string used for jobs topics.
 */
#define OTA_TOPIC_JOBS                          "jobs"

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define mqttexampleMAX_UINT32                   0xffffffff

/**
 * @brief The string used for streaming service topics.
 */
#define OTA_TOPIC_STREAM                        "streams"

/**
 * @brief The length of #OTA_TOPIC_PREFIX
 */
#define OTA_TOPIC_PREFIX_LENGTH                 ( ( uint16_t ) ( sizeof( OTA_TOPIC_PREFIX ) - 1U ) )

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
#define otaexampleKEEP_ALIVE_TIMEOUT_SECONDS    ( 60U )

/**
 * @brief Timeout for receiving CONNACK packet in milliseconds.
 */
#define otaexampleCONNACK_RECV_TIMEOUT_MS       ( 1000U )

/**
 * @brief OTA Library task stack size in words.
 */
#define otaexampleSTACK_SIZE                    ( 4096 )


/**
 * @brief Configure application version.
 */

#define APP_VERSION_MAJOR    0
#define APP_VERSION_MINOR    9
#define APP_VERSION_BUILD    2


/**
 * @brief Callback invoked by OTA to publish a message to the specified client/topic at the given QOS.
 * The function uses MQTT agent to queue the PUBLISH and then waits for a notification from agent on
 * successful publish
 *
 */
static OtaMqttStatus_t mqttPublish( const char * const pacTopic,
                                    uint16_t topicLen,
                                    const char * pMsg,
                                    uint32_t msgSize,
                                    uint8_t qos );

/**
 * @brief Callback invoked by OTA to subscribe to a topic filter.
 * The function uses MQTT agent to queue the SUBSCRIBE operation and then waits for a notification from agent for a
 * successfull subscribe. Function also registers a callback with the agent for topic filter. Any subsequent
 * packets on the topic matching this filter, is routed by the agent to this callback.
 */
static OtaMqttStatus_t mqttSubscribe( const char * pTopicFilter,
                                      uint16_t topicFilterLength,
                                      uint8_t ucQoS );

/**
 * @brief Callback invoked by OTA to unsubscribe from a topic filter.
 * The function uses MQTT agent to queue the UNSUBSCRIBE operation and then waits for a notification from agent for a
 * successfull unsubscribe. Function also registers a callback with the agent for topic filter. Any subsequent
 * packets on the topic matching this filter, is routed by the agent to this callback.
 */
static OtaMqttStatus_t mqttUnsubscribe( const char * pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t ucQoS );

/**
 * @brief Free OTA event buffer.
 * Function returns back the event buffer to the pool.
 */
static void otaEventBufferFree( OtaEventData_t * const pxBuffer );

/**
 * @brief Get an unused event buffer.
 * Function returns first available buffer from the pool.
 */
static OtaEventData_t * otaEventBufferGet( void );

/**
 * @brief Wrapper function to run the OTA agent task.
 */
static void otaAgentTaskWrapper( void * pvParam );

/**
 * @brief Callback invoked for packets received on data stream topic.
 * Function gets a free event buffer, and queues the data for processing by the OTA
 * agent task.
 */
static void mqttDataCallback( MQTTPublishInfo_t * pPublishInfo,
                              void * pxSubscriptionContext );

/**
 * @brief Callback invoked for packets received on job stream topic.
 * Function gets a free event buffer, and queues the job data for processing by the OTA
 * agent task.
 */
static void mqttJobCallback( MQTTPublishInfo_t * pPublishInfo,
                             void * pxSubscriptionContext );

/**
 * @brief Update File path buffer.
 */
static uint8_t updateFilePath[ otaexampleMAX_FILE_PATH_SIZE ];

/**
 * @brief Certificate File path buffer.
 */
static uint8_t certFilePath[ otaexampleMAX_FILE_PATH_SIZE ];

/**
 * @brief Stream name buffer.
 */
static uint8_t streamName[ otaexampleMAX_STREAM_NAME_SIZE ];

/**
 * @brief Decode memory.
 */
static uint8_t decodeMem[ ( 1U << otaconfigLOG2_FILE_BLOCK_SIZE ) ];

/**
 * @brief Bitmap memory.
 */
static uint8_t bitmap[ OTA_MAX_BLOCK_BITMAP_SIZE ];

/**
 * @brief Event buffer.
 */
static OtaEventData_t eventBuffer[ otaconfigMAX_NUM_OTA_DATA_BUFFERS ] = { 0 };

/*
 * @brief Semaphore used to manage the synchronization of OTA event buffers.
 */
static SemaphoreHandle_t xBufferSemaphore;

/**
 * @brief Static handle for MQTT context.
 */
static MQTTContextHandle_t xOTAMQTTContextHandle = 0;

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

/**
 * @brief The static callbacks for the topic filter types.
 */
static IncomingPublishCallback_t otaMessageCallback[ OtaNumOfMessageType ] = { mqttJobCallback, mqttDataCallback };
/*-----------------------------------------------------------*/

static void otaEventBufferFree( OtaEventData_t * const pxBuffer )
{
    if( xSemaphoreTake( xBufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        pxBuffer->bufferUsed = false;
        ( void ) xSemaphoreGive( xBufferSemaphore );
    }
    else
    {
        LogError( ( "Failed to get buffer semaphore." ) );
    }
}

/*-----------------------------------------------------------*/

static OtaEventData_t * otaEventBufferGet( void )
{
    uint32_t ulIndex = 0;
    OtaEventData_t * pFreeBuffer = NULL;

    if( xSemaphoreTake( xBufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        for( ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++ )
        {
            if( eventBuffer[ ulIndex ].bufferUsed == false )
            {
                eventBuffer[ ulIndex ].bufferUsed = true;
                pFreeBuffer = &eventBuffer[ ulIndex ];
                break;
            }
        }

        ( void ) xSemaphoreGive( xBufferSemaphore );
    }
    else
    {
        LogError( ( "Failed to get buffer semaphore." ) );
    }

    return pFreeBuffer;
}

/*-----------------------------------------------------------*/
static void otaAgentTaskWrapper( void * pvParam )
{
    OTA_EventProcessingTask( pvParam );
    vTaskDelete( NULL );
}

static void prvOTADemoTask(void* pvParam)
{
    /* OTA library packet statistics per job.*/
    OtaAgentStatistics_t otaStatistics = { 0 };

    /* OTA Agent state returned from calling OTA_GetAgentState.*/
    OtaState_t state = OtaAgentStateStopped;


    /*
     * Loops as long as OTA agent is active. Suspends the OTA download if it
     * identifies a network disconnect, and resumes OTA session once the network
     * is reconnected.
     */
    while (((state = OTA_GetState()) != OtaAgentStateStopped))
    {
        /* Get OTA statistics for currently executing job. */
        OTA_GetStatistics(&otaStatistics);
        LogInfo((" Received: %u   Queued: %u   Processed: %u   Dropped: %u",
            otaStatistics.otaPacketsReceived,
            otaStatistics.otaPacketsQueued,
            otaStatistics.otaPacketsProcessed,
            otaStatistics.otaPacketsDropped));

        vTaskDelay(pdMS_TO_TICKS(otaexampleTASK_DELAY_MS));
    }

    LogError(("OTA agent task stopped. Exiting OTA demo."));

    vTaskDelete(NULL);
}


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
 * @param[in] pData Data associated with the event.
 * @return None.
 */
static void otaAppCallback( OtaJobEvent_t event,
                            const void * pData )
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

        /*
         * Exit gracefully by signalling an OTA agent shutdown.
         */
        OTA_Shutdown( 0 );
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
    else if( event == OtaJobEventProcessed )
    {
        configASSERT( pData );
        otaEventBufferFree( ( OtaEventData_t * ) pData );
    }
}

/*-----------------------------------------------------------*/

static void mqttDataCallback( MQTTPublishInfo_t * pPublishInfo,
                              void * pxSubscriptionContext )
{
    configASSERT( pPublishInfo != NULL );

    ( void ) pxSubscriptionContext;

    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    LogInfo( ( "Received data message callback, size %d.\n\n", pPublishInfo->payloadLength ) );

    configASSERT( pPublishInfo->payloadLength <= OTA_DATA_BLOCK_SIZE );

    pData = otaEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedFileBlock;
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

static void mqttJobCallback( MQTTPublishInfo_t * pPublishInfo,
                             void * pxSubscriptionContext )
{
    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    ( void ) pxSubscriptionContext;

    configASSERT( pPublishInfo != NULL );

    LogInfo( ( "Received job message callback, size %d.\n\n", pPublishInfo->payloadLength ) );

    configASSERT( pPublishInfo->payloadLength <= OTA_DATA_BLOCK_SIZE );

    pData = otaEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
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

static void prvCommandCallback( void * pCommandContext,
                                MQTTStatus_t xReturnStatus )
{
    TaskHandle_t xTaskToNotify = ( TaskHandle_t ) pCommandContext;

    configASSERT( xTaskToNotify );
    xTaskNotify( xTaskToNotify, xReturnStatus, eSetValueWithOverwrite );
}

/*-----------------------------------------------------------*/
static MQTTStatus_t prvSubscribeToTopic( MQTTQoS_t xQoS,
                                         char * pcTopicFilter,
                                         IncomingPublishCallback_t pCallback )
{
    MQTTStatus_t mqttStatus;
    uint32_t ulNotifiedValue;
    MQTTSubscribeInfo_t xSubscribeInfo[ 1 ] = { 0 };
    BaseType_t result;

    TaskHandle_t xTaskHandle = xTaskGetCurrentTaskHandle();

    xSubscribeInfo[ 0 ].pTopicFilter = pcTopicFilter;
    xSubscribeInfo[ 0 ].topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );
    xSubscribeInfo[ 0 ].qos = xQoS;

    LogInfo( ( " Subscribing to topic filter: %s", pcTopicFilter ) );
    xTaskNotifyStateClear( NULL );

    mqttStatus = MQTTAgent_Subscribe( xOTAMQTTContextHandle,
                                      xSubscribeInfo,
                                      pCallback,
                                      NULL,
                                      prvCommandCallback,
                                      ( void * ) xTaskHandle,
                                      otaexampleMQTT_DELAY_MS );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( mqttStatus == MQTTSuccess )
    {
        result = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, &ulNotifiedValue, pdMS_TO_TICKS( otaexampleMQTT_DELAY_MS ) );

        if( result == pdTRUE )
        {
            mqttStatus = ( MQTTStatus_t ) ulNotifiedValue;
        }
        else
        {
            mqttStatus = MQTTRecvFailed;
        }
    }

    return mqttStatus;
}

static OtaMqttStatus_t mqttSubscribe( const char * pTopicFilter,
                                      uint16_t topicFilterLength,
                                      uint8_t ucQoS )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    OtaMessageType_t otaMessageType;
    MQTTStatus_t mqttStatus = MQTTBadParameter;

    configASSERT( pTopicFilter != NULL );
    configASSERT( topicFilterLength > 0 );

    otaMessageType = getOtaMessageType( pTopicFilter, topicFilterLength );
    configASSERT( otaMessageType < OtaNumOfMessageType );

    /* Send SUBSCRIBE packet. */
    mqttStatus = prvSubscribeToTopic( ucQoS,
                                      pTopicFilter,
                                      otaMessageCallback[ otaMessageType ] );

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to SUBSCRIBE to topic with error = %u.",
                    mqttStatus ) );

        otaRet = OtaMqttSubscribeFailed;
    }
    else
    {
        LogInfo( ( "SUBSCRIBE topic %.*s to broker.\n\n",
                   topicFilterLength,
                   pTopicFilter ) );

        otaRet = OtaMqttSuccess;
    }

    return otaRet;
}

/*
 * Publish a message to the specified client/topic at the given QOS.
 */
static OtaMqttStatus_t mqttPublish( const char * const pacTopic,
                                    uint16_t topicLen,
                                    const char * pMsg,
                                    uint32_t msgSize,
                                    uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    BaseType_t result;
    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTPublishInfo_t publishInfo = { 0 };
    TaskHandle_t xTaskHandle;
    uint32_t ulNotifiedValue;

    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = qos;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    xTaskHandle = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear( NULL );

    mqttStatus = MQTTAgent_Publish( xOTAMQTTContextHandle,
                                    &publishInfo,
                                    prvCommandCallback,
                                    ( void * ) xTaskHandle,
                                    otaexampleMQTT_DELAY_MS );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( mqttStatus == MQTTSuccess )
    {
        result = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, &ulNotifiedValue, pdMS_TO_TICKS( otaexampleMQTT_DELAY_MS ) );

        if( result != pdTRUE )
        {
            mqttStatus = MQTTSendFailed;
        }
        else
        {
            mqttStatus = ( MQTTStatus_t ) ( ulNotifiedValue );
        }
    }

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send PUBLISH packet to broker with error = %u.", mqttStatus ) );
        otaRet = OtaMqttPublishFailed;
    }
    else
    {
        LogInfo( ( "Sent PUBLISH packet to broker %.*s to broker.\n\n",
                   topicLen,
                   pacTopic ) );

        otaRet = OtaMqttSuccess;
    }

    return otaRet;
}

static MQTTStatus_t prvUnSubscribeFromTopic( MQTTQoS_t xQoS,
                                             char * pcTopicFilter )
{
    MQTTStatus_t mqttStatus;
    uint32_t ulNotifiedValue;
    MQTTSubscribeInfo_t xSubscribeInfo[ 1 ] = { 0 };
    int index = 0;
    BaseType_t result;

    TaskHandle_t xTaskHandle = xTaskGetCurrentTaskHandle();

    xSubscribeInfo[ index ].pTopicFilter = pcTopicFilter;
    xSubscribeInfo[ index ].topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );
    xSubscribeInfo[ index ].qos = xQoS;

    LogInfo( ( " Unsubscribing to topic filter: %s", pcTopicFilter ) );
    xTaskNotifyStateClear( NULL );


    mqttStatus = MQTTAgent_Unsubscribe( xOTAMQTTContextHandle,
                                        xSubscribeInfo,
                                        prvCommandCallback,
                                        ( void * ) xTaskHandle,
                                        otaexampleMQTT_DELAY_MS );

    /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
     * duration of the command. */
    if( mqttStatus == MQTTSuccess )
    {
        result = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, &ulNotifiedValue, pdMS_TO_TICKS( otaexampleMQTT_DELAY_MS ) );

        if( result == pdTRUE )
        {
            mqttStatus = ( MQTTStatus_t ) ( ulNotifiedValue );
        }
        else
        {
            mqttStatus = MQTTRecvFailed;
        }
    }

    return mqttStatus;
}

static OtaMqttStatus_t mqttUnsubscribe( const char * pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t ucQoS )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    MQTTStatus_t mqttStatus = MQTTBadParameter;

    configASSERT( pTopicFilter != NULL );
    configASSERT( topicFilterLength > 0 );

    /* Send SUBSCRIBE packet. */
    mqttStatus = prvUnSubscribeFromTopic( ucQoS,
                                          pTopicFilter );

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to UNSUBSCRIBE from topic %.*s with error = %u.",
                    topicFilterLength,
                    pTopicFilter,
                    mqttStatus ) );

        otaRet = OtaMqttUnsubscribeFailed;
    }
    else
    {
        LogInfo( ( "UNSUBSCRIBED from topic %.*s.\n\n",
                   topicFilterLength,
                   pTopicFilter ) );

        otaRet = OtaMqttSuccess;
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

void vStartOTACodeSigningDemo(configSTACK_DEPTH_TYPE uxStackSize,
    UBaseType_t uxPriority)
{

    /* FreeRTOS APIs return status. */
    BaseType_t xResult = pdPASS;

    /* OTA library return status. */
    OtaErr_t otaRet = OtaErrNone;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t eventMsg = { 0 };

    /* OTA interface context required for library interface functions.*/
    OtaInterfaces_t otaInterfaces;

    /* Set OTA Library interfaces.*/
    setOtaInterfaces(&otaInterfaces);

    LogInfo(("OTA over MQTT demo, Application version %u.%u.%u",
        appFirmwareVersion.u.x.major,
        appFirmwareVersion.u.x.minor,
        appFirmwareVersion.u.x.build));


    xBufferSemaphore = xSemaphoreCreateMutex();

    if (xBufferSemaphore == NULL)
    {
        xResult = pdFAIL;
    }

    if (xResult == pdPASS)
    {
        memset(eventBuffer, 0x00, sizeof(eventBuffer));

        if ((otaRet = OTA_Init(&otaBuffer,
            &otaInterfaces,
            (const uint8_t*)(democonfigCLIENT_IDENTIFIER),
            otaAppCallback)) != OtaErrNone)
        {
            LogError(("Failed to initialize OTA Agent, exiting = %u.",
                otaRet));
            xResult = pdFAIL;
        }
    }

    /****************************** Create OTA Task. ******************************/

    if (xResult == pdPASS)
    {
        if ((xResult = xTaskCreate(otaAgentTaskWrapper,
            "OTA Agent Task",
            otaexampleSTACK_SIZE,
            NULL,
            OTA_AGENT_TASK_PRIORITY,
            NULL)) != pdPASS)
        {
            LogError(("Failed to start OTA task: "
                ",errno=%d",
                xResult));
        }
    }

    /***************************Start OTA demo loop. ******************************/

    if (xResult == pdPASS)
    {
        /* Start the OTA Agent.*/
        eventMsg.eventId = OtaAgentEventStart;
        OTA_SignalEvent(&eventMsg);
    }

    if (xResult == pdPASS)
    {
        if ((xResult = xTaskCreate(prvOTADemoTask,
            "OTA Agent Task",
            uxStackSize,
            NULL,
            uxPriority,
            NULL)) != pdPASS)
        {
            LogError(("Failed to start OTA demo task: "
                ",errno=%d",
                xResult));
        }
    }

}

void vSuspendOTAUpdate( void )
{
    if( ( OTA_GetState() != OtaAgentStateSuspended ) && ( OTA_GetState() != OtaAgentStateStopped ) )
    {
        OTA_Suspend();

        while( ( OTA_GetState() != OtaAgentStateSuspended ) &&
               ( OTA_GetState() != OtaAgentStateStopped ) )
        {
            vTaskDelay( pdMS_TO_TICKS( otaexampleTASK_DELAY_MS ) );
        }
    }
}

void vResumeOTAUpdate( void )
{
    if( OTA_GetState() == OtaAgentStateSuspended )
    {
        OTA_Resume();

        while( OTA_GetState() == OtaAgentStateSuspended )
        {
            vTaskDelay( pdMS_TO_TICKS( otaexampleTASK_DELAY_MS ) );
        }
    }
}
