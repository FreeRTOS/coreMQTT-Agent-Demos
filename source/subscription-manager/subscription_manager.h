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
 * @file subscription_manager.h
 * @brief Functions for managing MQTT subscriptions.
 */
#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

/* Kernel includes. */
#include "FreeRTOS.h"

/* core MQTT include. */
#include "core_mqtt.h"

/**
 * @brief Callback function called when receiving a publish.
 *
 * @param[in] pvIncomingPublishCallbackContext The incoming publish callback context.
 * @param[in] pxPublishInfo Deserialized publish information.
 */
typedef void (* IncomingPubCallback_t )( void * pvIncomingPublishCallbackContext,
                                         MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief Add a subscription to the subscription list.
 *
 * @note Multiple tasks can be subscribed to the same topic with different
 * context-callback pairs. However, a single context-callback pair may only be
 * associated to the same topic filter once.
 *
 * @param[in] pcTopicFilterString Topic filter string of subscription.
 * @param[in] uTopicFilterLength Length of topic filter string.
 * @param[in] pxIncomingPublishCallback Callback function for the subscription.
 * @param[in] pvIncomingPublishCallbackContext Context for the subscription callback.
 *
 * @return `true` if subscription added or exists, `false` if insufficient memory.
 */
BaseType_t addSubscription( const char * pcTopicFilterString,
                            uint16_t uTopicFilterLength,
                            IncomingPubCallback_t pxIncomingPublishCallback,
                            void * pvIncomingPublishCallbackContext );

/**
 * @brief Remove a subscription from the subscription list.
 *
 * @note If the topic filter exists multiple times in the subscription list,
 * then every instance of the subscription will be removed.
 *
 * @param[in] pcTopicFilterString Topic filter of subscription.
 * @param[in] uTopicFilterLength Length of topic filter.
 */
void removeSubscription( const char * pcTopicFilterString,
                         uint16_t uTopicFilterLength );

/**
 * @brief Handle incoming publishes by invoking the callbacks registered
 * for the incoming publish's topic filter.
 *
 * @param[in] pxPublishInfo Info of incoming publish.
 *
 * @return pdTRUE if an application callback could be invoked;
 *  pdFALSE otherwise.
 */
BaseType_t handleIncomingPublishes( MQTTPublishInfo_t * pxPublishInfo );

#endif /* MQTT_AGENT_H */
