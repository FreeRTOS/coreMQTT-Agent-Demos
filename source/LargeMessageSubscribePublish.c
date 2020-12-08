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

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo Specific configs. */
#include "demo_config.h"

/* MQTT library includes. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

/* MQTT agent include. */
#include "freertos_mqtt_agent.h"

/**
 * @brief Ticks to wait for task notifications.
 */
#define mqttexampleDEMO_TICKS_TO_WAIT                pdMS_TO_TICKS( 5000 )

/**
 * @brief Delay for the synchronous publisher task between publishes.
 */
#define mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS      25U

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define mqttexampleMAX_UINT32                       0xffffffff

#define mqttexamplePROTOCOL_OVERHEAD 50
#define mqttexampleMAX_PAYLOAD_LENGTH ( mqttexampleNETWORK_BUFFER_SIZE - mqttexamplePROTOCOL_OVERHEAD )

/*-----------------------------------------------------------*/

struct CommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
};

/**
 * @brief Common callback for commands in this demo.
 *
 * This callback marks the command as complete and notifies the calling task.
 *
 * @param[in] pxCommandContext Context of the initial command.
 */
static void prvCommandCallback( CommandContext_t * pxCommandContext,
                                MQTTStatus_t xReturnStatus );

static void prvSubscribeCommandCallback( void *pxCommandContext,
                                         MQTTStatus_t xReturnStatus );

static BaseType_t prvWaitForCommandAcknowledgment( uint32_t *pulNotifiedValue );

static void prvWriteIncomingPayloadToBuffer( MQTTPublishInfo_t * pxPublishInfo,
                                             void * pxSubscriptionContext );

/*-----------------------------------------------------------*/

/**
 * @brief Global MQTT context handle - use context 0.
 */
static const MQTTContextHandle_t xMQTTContextHandle = 0;

static void prvCommandCallback( CommandContext_t * pxCommandContext,
                                MQTTStatus_t xReturnStatus )
{
    pxCommandContext->xReturnStatus = xReturnStatus;

    if( pxCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pxCommandContext->xTaskToNotify, pxCommandContext->ulNotificationValue, eSetValueWithOverwrite );
    }
}

static void prvSubscribeCommandCallback( void *pxCommandContext,
                                         MQTTStatus_t xReturnStatus ) /*_RB_ Do we need the packet ID here so we know which subscribe is being acked? */
{
CommandContext_t *pxApplicationDefinedContext = ( CommandContext_t * ) pxCommandContext;

    configASSERT( pxCommandContext );

    /* Store the result in the application defined context. */
    pxApplicationDefinedContext->xReturnStatus = xReturnStatus;
    xTaskNotify( pxApplicationDefinedContext->xTaskToNotify, xReturnStatus, eSetValueWithOverwrite );
}

static BaseType_t prvWaitForCommandAcknowledgment( uint32_t *pulNotifiedValue )
{
    BaseType_t xReturn;

    /* Wait for this task to get notified. */
    xReturn = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, pulNotifiedValue, mqttexampleDEMO_TICKS_TO_WAIT );
    return xReturn;
}

static void prvWriteIncomingPayloadToBuffer( MQTTPublishInfo_t * pxPublishInfo,
                                             void * pxSubscriptionContext )
{
    CommandContext_t *xApplicationDefinedContext = ( CommandContext_t * ) pxSubscriptionContext;

    configASSERT( pxPublishInfo->payloadLength <= mqttexampleMAX_PAYLOAD_LENGTH );
    memcpy( ( void * ) xApplicationDefinedContext->ulNotificationValue, ( void * ) pxPublishInfo->pPayload, pxPublishInfo->payloadLength );
    xTaskNotify( xApplicationDefinedContext->xTaskToNotify, 0x00, eSetValueWithOverwrite );
}


void vLargeMessageSubscribePublishTask( void * pvParameters )
{
    static char maxPayloadMessage[ mqttexampleMAX_PAYLOAD_LENGTH ];
    static char receivedEchoesPayload[ mqttexampleMAX_PAYLOAD_LENGTH ];
    MQTTPublishInfo_t xPublishInfo = { 0 };
    BaseType_t x;
    char c = '0';
    const char* topicBuf = "/max/payload/message";
    volatile CommandContext_t xApplicationDefinedContext = { 0 };
    BaseType_t xCommandAdded;
    MQTTSubscribeInfo_t xSubscribeInfo;

    ( void ) pvParameters;

    /* Create a large buffer of data. */
    for( x = 0; x < mqttexampleNETWORK_BUFFER_SIZE; x++ )
    {
        if( x < mqttexampleNETWORK_BUFFER_SIZE / 2 )
		{
            maxPayloadMessage[ x ] = 'a';
		}
        else
		{
            maxPayloadMessage[ x ] = 'b';
		}
        c++;

        /* Keep the characters human readable. */
        if( c == '~' )
        {
            c = '0';
        }
    }

//////////

    xApplicationDefinedContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xApplicationDefinedContext.ulNotificationValue = ( uint32_t ) receivedEchoesPayload;

    /* Complete the subscribe information.  The topic string must persist for
    duration of subscription! */
    xSubscribeInfo.pTopicFilter = topicBuf;
    xSubscribeInfo.topicFilterLength = ( uint16_t ) strlen( topicBuf );
    xSubscribeInfo.qos = MQTTQoS1;

    /* Loop in case the queue used to communicate with the MQTT agent is full and
     * attempts to post to it time out.  The queue will not become full if the
     * priority of the MQTT agent task is higher than the priority of the task
     * calling this function. */
    xTaskNotifyStateClear( NULL );
    LogInfo( ( "Sending subscribe request to agent for topic filter: %s", topicBuf ) );
    do
    {
        xCommandAdded = MQTTAgent_Subscribe( xMQTTContextHandle,
                                             &( xSubscribeInfo ),
                                             prvWriteIncomingPayloadToBuffer,
                                             ( void * ) &xApplicationDefinedContext,
                                             prvSubscribeCommandCallback,
                                             ( void * ) &xApplicationDefinedContext );
    } while( xCommandAdded == false );

    /* Always wait for acks from subscribe messages. */
    xCommandAdded = prvWaitForCommandAcknowledgment( NULL );
    configASSERT( xCommandAdded == pdTRUE );
    LogInfo( ( "Received subscribe ack for topic %s", topicBuf ) );

    memset( ( void * ) &xPublishInfo, 0x00, sizeof( xPublishInfo ) );
    xPublishInfo.qos = MQTTQoS1;
    xPublishInfo.pTopicName = topicBuf;
    xPublishInfo.topicNameLength = ( uint16_t ) strlen( topicBuf );
    xPublishInfo.pPayload = maxPayloadMessage;
    xPublishInfo.payloadLength = mqttexampleMAX_PAYLOAD_LENGTH;

    for( ;; )
    {
        /* Clear out the buffer used to receive incoming publishes. */
        memset( receivedEchoesPayload, 0x00, sizeof receivedEchoesPayload );

        xTaskNotifyStateClear( NULL );

        /* Publish to the topic to which this task is also subscribed to receive an
        echo back. */
        LogInfo( ( "Sending large publish request to agent with message on topic \"%s\"", topicBuf ) );
        xCommandAdded = MQTTAgent_Publish( xMQTTContextHandle, &xPublishInfo, prvCommandCallback, &xApplicationDefinedContext );
        configASSERT( xCommandAdded == pdTRUE );

        LogInfo( ( "Waiting for large publish to topic %s to complete.", topicBuf ) );
        prvWaitForCommandAcknowledgment( NULL );

        /* Wait for the publish back to this task. */
        xTaskNotifyWait( 0x00, 0x00, NULL, mqttexampleDEMO_TICKS_TO_WAIT );

        x = memcmp( maxPayloadMessage, receivedEchoesPayload, mqttexampleMAX_PAYLOAD_LENGTH );
        configASSERT( x == 0 );
        if( x == 0 )
        {
            LogInfo( ( "Received ack from publishing to topic %s. Sleeping for %d ms.", topicBuf, mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS ) );
        }
        else
        {
            LogError( ( "Error - Timed out or didn't receive ack from publishing to topic %s Sleeping for %d ms.", topicBuf, mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS ) );
        }

        vTaskDelay( pdMS_TO_TICKS( mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS * 5 ) );
    }

    vTaskDelete( NULL );
}