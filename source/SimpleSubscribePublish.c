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
#define mqttexampleDEMO_TICKS_TO_WAIT                pdMS_TO_TICKS( 10000 )

/**
 * @brief Size of statically allocated buffers for holding topic names and payloads.
 */
#define mqttexampleDEMO_BUFFER_SIZE                  100

/**
 * @brief Delay for the synchronous publisher task between publishes.
 */
#define mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS      25U

/**
 * @brief Number of publishes done by each task in this demo.
 */
#define mqttexamplePUBLISH_COUNT                     0xffffffff /*_RB_ Just while testing the OTA. */

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define mqttexampleMAX_UINT32                        0xffffffff

/**
 * @brief The maximum amount of time in milliseconds to wait for the command to be
 * posted to the MQTT agent should the MQTT agent's command queue be full.  Tasks
 * wait in the Blocked state, so don't use any CPU time.
 */
#define mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS    200
/*-----------------------------------------------------------*/

struct CommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
};

/*-----------------------------------------------------------*/

/**
 * @brief Common callback for commands in this demo.
 *
 * This callback marks the command as complete and notifies the calling task.
 *
 * @param[in] pxCommandContext Context of the initial command.
 */
static void prvCommandCallback( CommandContext_t * pxCommandContext,
                                MQTTStatus_t xReturnStatus );

static BaseType_t prvWaitForCommandAcknowledgment( uint32_t * pulNotifiedValue );

static void prvPrintIncomingPublish( MQTTPublishInfo_t * pxPublishInfo,
                                     void * pvNotUsed );

static bool prvSubscribeToTopic( MQTTQoS_t xQoS,
                                 char * pcTopicFilter );

/*-----------------------------------------------------------*/

/* Use global context handle. */
extern MQTTContextHandle_t xMQTTContextHandle;

/*-----------------------------------------------------------*/

static void prvCommandCallback( CommandContext_t * pxCommandContext,
                                MQTTStatus_t xReturnStatus )
{
    pxCommandContext->xReturnStatus = xReturnStatus;

    if( pxCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pxCommandContext->xTaskToNotify, pxCommandContext->ulNotificationValue, eSetValueWithOverwrite );
    }
}

/*-----------------------------------------------------------*/

static void prvSubscribeCommandCallback( void * pxCommandContext,
                                         MQTTStatus_t xReturnStatus ) /*_RB_ Do we need the packet ID here so we know which subscribe is being acked? */
{
    CommandContext_t * pxApplicationDefinedContext = ( CommandContext_t * ) pxCommandContext;

    configASSERT( pxCommandContext );

    /* Store the result in the application defined context. */
    pxApplicationDefinedContext->xReturnStatus = xReturnStatus;
    xTaskNotify( pxApplicationDefinedContext->xTaskToNotify, xReturnStatus, eSetValueWithOverwrite );
}

/*-----------------------------------------------------------*/

static BaseType_t prvWaitForCommandAcknowledgment( uint32_t * pulNotifiedValue )
{
    BaseType_t xReturn;

    /* Wait for this task to get notified. */
    xReturn = xTaskNotifyWait( 0, mqttexampleMAX_UINT32, pulNotifiedValue, mqttexampleDEMO_TICKS_TO_WAIT );
    return xReturn;
}

/*-----------------------------------------------------------*/

static void prvPrintIncomingPublish( MQTTPublishInfo_t * pxPublishInfo, /*_RB_ Are these parameters the other way round in the coreMQTT library? */
                                     void * pvNotUsed )
{
    static char cTerminatedString[ mqttexampleDEMO_BUFFER_SIZE ];

    /* Although pvNotUsed is not used it was set to NULL, so check it has its
     * expected value. */
    configASSERT( pvNotUsed == NULL );

    if( pxPublishInfo->payloadLength < mqttexampleDEMO_BUFFER_SIZE )
    {
        memcpy( ( void * ) cTerminatedString, pxPublishInfo->pPayload, pxPublishInfo->payloadLength );
        cTerminatedString[ pxPublishInfo->payloadLength ] = 0x00;
    }
    else
    {
        memcpy( ( void * ) cTerminatedString, pxPublishInfo->pPayload, mqttexampleDEMO_BUFFER_SIZE );
        cTerminatedString[ mqttexampleDEMO_BUFFER_SIZE - 1 ] = 0x00;
    }

    LogInfo( ( "Received incoming publish message %s", cTerminatedString ) );
}

/*-----------------------------------------------------------*/

static bool prvSubscribeToTopic( MQTTQoS_t xQoS,
                                 char * pcTopicFilter )
{
    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;
    uint32_t ulSubscribeMessageID;
    MQTTSubscribeInfo_t xSubscribeInfo;
    static int32_t ulNextSubscribeMessageID = 0;
    volatile CommandContext_t xApplicationDefinedContext = { 0 };

    /* Create a unique number of the subscribe that is about to be sent.  The number
     * is used as the command context and is sent back to this task as a notification
     * in the callback that executed upon receipt of the subscription acknowledgment.
     * That way this task can match an acknowledgment to a subscription. */
    xTaskNotifyStateClear( NULL );
    taskENTER_CRITICAL();
    {
        ulNextSubscribeMessageID++;
        ulSubscribeMessageID = ulNextSubscribeMessageID;
    }
    taskEXIT_CRITICAL();

    /* Complete the subscribe information.  The topic string must persist for
     * duration of subscription! */
    xSubscribeInfo.pTopicFilter = pcTopicFilter;
    xSubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );
    xSubscribeInfo.qos = xQoS;

    /* Complete an application defined context associated with this subscribe message.
     * This gets updated in the callback function so the variable must persist until
     * the callback executes. */
    xApplicationDefinedContext.ulNotificationValue = ulNextSubscribeMessageID;
    xApplicationDefinedContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    /* Loop in case the queue used to communicate with the MQTT agent is full and
     * attempts to post to it time out.  The queue will not become full if the
     * priority of the MQTT agent task is higher than the priority of the task
     * calling this function. */
    LogInfo( ( "Sending subscribe request to agent for topic filter: %s with id %d", pcTopicFilter, ( int ) ulSubscribeMessageID ) );

    do
    {
        xCommandAdded = MQTTAgent_Subscribe( xMQTTContextHandle,
                                             &( xSubscribeInfo ),
                                             prvPrintIncomingPublish,
                                             NULL,
                                             prvSubscribeCommandCallback,
                                             ( void * ) &xApplicationDefinedContext,
                                             mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS );
    } while( xCommandAdded != MQTTSuccess );

    /* Always wait for acks from subscribe messages. */
    xCommandAcknowledged = prvWaitForCommandAcknowledgment( NULL );

    if( xCommandAcknowledged == MQTTSuccess )
    {
        LogInfo( ( "Timed out waiting for ack to subscribe message topic %s", pcTopicFilter ) );
    }
    else
    {
        if( ( xApplicationDefinedContext.ulNotificationValue == ulSubscribeMessageID ) && ( xApplicationDefinedContext.xReturnStatus == MQTTSuccess ) )
        {
            LogInfo( ( "Received subscribe ack for topic %s", pcTopicFilter ) );
        }
        else
        {
            LogInfo( ( "Error failed to subscribe to topic %s", pcTopicFilter ) );
        }
    }

    return xCommandAcknowledged;
}

/*-----------------------------------------------------------*/

void vSimpleSubscribePublishTask( void * pvParameters )
{
    MQTTPublishInfo_t xPublishInfo = { 0 };
    char payloadBuf[ mqttexampleDEMO_BUFFER_SIZE ];
    char topicBuf[ mqttexampleDEMO_BUFFER_SIZE ]; /*_RB_ Must persist until publish ack received. */
    char taskName[ mqttexampleDEMO_BUFFER_SIZE ];
    CommandContext_t xCommandContext;
    uint32_t ulNotification = 0U, ulValueToNotify = 0UL;
    MQTTStatus_t xCommandAdded;
    uint32_t ulTaskNumber = ( uint32_t ) pvParameters;
    MQTTQoS_t xQoS;

    /* Have different tasks use different QoS.  3 as there are 3 QoS options, 0 to 2. */
    xQoS = ( MQTTQoS_t ) ( ulTaskNumber % 2UL );

    /* Create a unique name for this task from the task number that is passed into
     * the task using the task's parameter. */
    snprintf( taskName, mqttexampleDEMO_BUFFER_SIZE, "Publisher%d", ( int ) ulTaskNumber );

    /* Create a topic name for this task to publish to. */
    snprintf( topicBuf, mqttexampleDEMO_BUFFER_SIZE, "/filter/%s", taskName );

    /* Subscribe to the same topic to which this task will publish.  That will
     * result in each published message being published from the server back to us. */
    prvSubscribeToTopic( xQoS, topicBuf );

    /* Use the task number as the QoS so different tasks are testing different QoS. */
    memset( ( void * ) &xPublishInfo, 0x00, sizeof( xPublishInfo ) );
    xPublishInfo.qos = xQoS;
    xPublishInfo.pTopicName = topicBuf;
    xPublishInfo.topicNameLength = ( uint16_t ) strlen( topicBuf );
    xPublishInfo.pPayload = payloadBuf;

    memset( ( void * ) &xCommandContext, 0x00, sizeof( xCommandContext ) );
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    /* Synchronous publishes. */
    for( ulValueToNotify = 0UL; ulValueToNotify < mqttexamplePUBLISH_COUNT; ulValueToNotify++ )
    {
        snprintf( payloadBuf, mqttexampleDEMO_BUFFER_SIZE, "%s publishing message %d", taskName, ( int ) ulValueToNotify );
        xPublishInfo.payloadLength = ( uint16_t ) strlen( payloadBuf );

        xCommandContext.ulNotificationValue = ulValueToNotify;

        LogInfo( ( "Sending publish request to agent with message \"%s\" on topic \"%s\"", payloadBuf, topicBuf ) );
        xCommandAdded = MQTTAgent_Publish( xMQTTContextHandle,
                                           &xPublishInfo,
                                           prvCommandCallback,
                                           &xCommandContext,
                                           mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS );
        configASSERT( xCommandAdded == MQTTSuccess );

        /* For QoS 1 and 2, wait for the publish acknowledgment.  For QoS0, wait for
         * the publish to be sent. */
        ulNotification = ~ulValueToNotify; /* To ensure ulNotification doesn't accidentally hold the expected value. */
        LogInfo( ( "Task %s waiting for publish %d to complete.", taskName, ulValueToNotify ) );
        prvWaitForCommandAcknowledgment( &ulNotification );

        if( ulNotification == ulValueToNotify )
        {
            LogInfo( ( "Received ack from publishing to topic %s. Sleeping for %d ms.", topicBuf, mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS ) );
        }
        else
        {
            LogInfo( ( "Error - Timed out or didn't receive ack from publishing to topic %s Sleeping for %d ms.", topicBuf, mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS ) );
            configASSERT( ulNotification == ulValueToNotify );
        }

        vTaskDelay( pdMS_TO_TICKS( mqttDELAY_BETWEEN_PUBLISH_OPERATIONS_MS ) );
    }

    /* Delete the task if it is complete. */
    vTaskDelete( NULL );
}
