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

/* Subscription manager header include. */
#include "subscription_manager.h"


BaseType_t addSubscription( SubscriptionElement_t * pxSubscriptionList,
                            const char * pcTopicFilterString,
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
        if( pxSubscriptionList[ lIndex ].uFilterStringLength == 0 )
        {
            xAvailableIndex = lIndex;
        }
        else if( ( pxSubscriptionList[ lIndex ].uFilterStringLength == uTopicFilterLength ) &&
                 ( strncmp( pcTopicFilterString, pxSubscriptionList[ lIndex ].pcSubscriptionFilterString, uTopicFilterLength ) == 0 ) )
        {
            /* If a subscription already exists, don't do anything. */
            if( ( pxSubscriptionList[ lIndex ].pxIncomingPublishCallback == pxIncomingPublishCallback ) &&
                ( pxSubscriptionList[ lIndex ].pvIncomingPublishCallbackContext == pvIncomingPublishCallbackContext ) )
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
        pxSubscriptionList[ xAvailableIndex ].pcSubscriptionFilterString = pcTopicFilterString;
        pxSubscriptionList[ xAvailableIndex ].uFilterStringLength = uTopicFilterLength;
        pxSubscriptionList[ xAvailableIndex ].pxIncomingPublishCallback = pxIncomingPublishCallback;
        pxSubscriptionList[ xAvailableIndex ].pvIncomingPublishCallbackContext = pvIncomingPublishCallbackContext;
        xReturnStatus = pdTRUE;
    }

    return xReturnStatus;
}

/*-----------------------------------------------------------*/

void removeSubscription( SubscriptionElement_t * pxSubscriptionList,
                         const char * topicFilterString,
                         uint16_t topicFilterLength )
{
    int32_t lIndex = 0;

    for( lIndex = 0; lIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS; lIndex++ )
    {
        if( pxSubscriptionList[ lIndex ].uFilterStringLength == topicFilterLength )
        {
            if( strncmp( pxSubscriptionList[ lIndex ].pcSubscriptionFilterString, topicFilterString, topicFilterLength ) == 0 )
            {
                memset( &( pxSubscriptionList[ lIndex ] ), 0x00, sizeof( SubscriptionElement_t ) );
            }
        }
    }
}

/*-----------------------------------------------------------*/

BaseType_t handleIncomingPublishes( SubscriptionElement_t * pxSubscriptionList,
                                    MQTTPublishInfo_t * pxPublishInfo )
{
    BaseType_t xCallbackInvoked = pdFALSE;
    int32_t lIndex = 0;
    bool isMatched = false;

    for( lIndex = 0; lIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS; lIndex++ )
    {
        if( pxSubscriptionList[ lIndex ].uFilterStringLength > 0 )
        {
            MQTT_MatchTopic( pxPublishInfo->pTopicName,
                             pxPublishInfo->topicNameLength,
                             pxSubscriptionList[ lIndex ].pcSubscriptionFilterString,
                             pxSubscriptionList[ lIndex ].uFilterStringLength,
                             &isMatched );

            if( isMatched == true )
            {
                pxSubscriptionList[ lIndex ].pxIncomingPublishCallback( pxSubscriptionList[ lIndex ].pvIncomingPublishCallbackContext,
                                                                        pxPublishInfo );
                xCallbackInvoked = pdTRUE;
            }
        }
    }

    return xCallbackInvoked;
}
