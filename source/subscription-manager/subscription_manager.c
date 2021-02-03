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
 * https://www.FreeRTOS.org
 * https://aws.amazon.com/freertos
 *
 */

/**
 * @file subscription_manager.c
 * @brief Functions for managing MQTT subscriptions.
 */

/* Demo config include. */
#include "demo_config.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/*-----------------------------------------------------------*/

/**
 * @brief Maximum number of subscriptions maintained by the subscription manager
 * simultaneously.
 */
#ifndef SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS
    #define SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS    ( 10U )
#endif

/*-----------------------------------------------------------*/

/**
 * @brief An element in the list of subscriptions maintained by the agent.
 *
 * @note The agent allows multiple tasks to subscribe to the same topic.
 * In this case, another element is added to the subscription list, differing
 * in the intended publish callback.
 */
typedef struct subscriptionElement
{
    IncomingPubCallback_t pxIncomingPublishCallback;
    void * pvIncomingPublishCallbackContext;
    uint16_t uFilterStringLength;
    const char * pcSubscriptionFilterString;
} SubscriptionElement_t;

/*-----------------------------------------------------------*/

/**
 * @brief The static array of subscription elements.
 *
 * @note No thread safety is required to this array, since the updates the array
 * elements are done only from one task at a time.
 */
static SubscriptionElement_t xSubscriptionList[ SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS ];

/*-----------------------------------------------------------*/

BaseType_t addSubscription( const char * pcTopicFilterString,
                            uint16_t uTopicFilterLength,
                            IncomingPubCallback_t pxIncomingPublishCallback,
                            void * pvIncomingPublishCallbackContext )
{
    int32_t lIndex = 0;
    size_t xAvailableIndex = SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS;
    BaseType_t xReturnStatus = pdFALSE;

    /* Start at end of array, so that we will insert at the first available index.
     * Scans backwards to find duplicates. */
    for( lIndex = ( int32_t ) SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS - 1; lIndex >= 0; lIndex-- )
    {
        if( xSubscriptionList[ lIndex ].uFilterStringLength == 0 )
        {
            xAvailableIndex = lIndex;
        }
        else if( ( xSubscriptionList[ lIndex ].uFilterStringLength == uTopicFilterLength ) &&
                 ( strncmp( pcTopicFilterString, xSubscriptionList[ lIndex ].pcSubscriptionFilterString, uTopicFilterLength ) == 0 ) )
        {
            /* If a subscription already exists, don't do anything. */
            if( ( xSubscriptionList[ lIndex ].pxIncomingPublishCallback == pxIncomingPublishCallback ) &&
                ( xSubscriptionList[ lIndex ].pvIncomingPublishCallbackContext == pvIncomingPublishCallbackContext ) )
            {
                LogWarn( ( "Subscription already exists.\n" ) );
                xAvailableIndex = SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS;
                xReturnStatus = pdTRUE;
                break;
            }
        }
    }

    if( ( xAvailableIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS ) && ( pxIncomingPublishCallback != NULL ) )
    {
        xSubscriptionList[ xAvailableIndex ].pcSubscriptionFilterString = pcTopicFilterString;
        xSubscriptionList[ xAvailableIndex ].uFilterStringLength = uTopicFilterLength;
        xSubscriptionList[ xAvailableIndex ].pxIncomingPublishCallback = pxIncomingPublishCallback;
        xSubscriptionList[ xAvailableIndex ].pvIncomingPublishCallbackContext = pvIncomingPublishCallbackContext;
        xReturnStatus = pdTRUE;
    }

    return xReturnStatus;
}

/*-----------------------------------------------------------*/

static void removeSubscription( const char * topicFilterString,
                                uint16_t topicFilterLength )
{
    int32_t lIndex = 0;

    for( lIndex = 0; lIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS; lIndex++ )
    {
        if( xSubscriptionList[ lIndex ].uFilterStringLength == topicFilterLength )
        {
            if( strncmp( xSubscriptionList[ lIndex ].pcSubscriptionFilterString, topicFilterString, topicFilterLength ) == 0 )
            {
                memset( &( xSubscriptionList[ lIndex ] ), 0x00, sizeof( SubscriptionElement_t ) );
            }
        }
    }
}

/*-----------------------------------------------------------*/

BaseType_t handleIncomingPublishes( MQTTPublishInfo_t * pxPublishInfo )
{
    BaseType_t xCallbackInvoked = pdFALSE;
    int32_t lIndex = 0;
    bool isMatched = false;

    for( lIndex = 0; lIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS; lIndex++ )
    {
        if( xSubscriptionList[ lIndex ].uFilterStringLength > 0 )
        {
            MQTT_MatchTopic( pxPublishInfo->pTopicName,
                             pxPublishInfo->topicNameLength,
                             xSubscriptionList[ lIndex ].pcSubscriptionFilterString,
                             xSubscriptionList[ lIndex ].uFilterStringLength,
                             &isMatched );

            if( isMatched == true )
            {
                xSubscriptionList[ lIndex ].pxIncomingPublishCallback( xSubscriptionList[ lIndex ].pvIncomingPublishCallbackContext,
                                                                       pxPublishInfo );
                xCallbackInvoked = pdTRUE;
            }
        }
    }

    return xCallbackInvoked;
}
