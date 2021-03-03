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
 * @file mqtt_agent_command_functions.h
 * @brief Functions for processing an MQTT agent command.
 */
#ifndef MQTT_AGENT_COMMAND_FUNCTIONS_H
#define MQTT_AGENT_COMMAND_FUNCTIONS_H

/* MQTT Agent include. */
#include "mqtt_agent.h"

#ifndef MQTT_AGENT_FUNCTION_TABLE
    #define MQTT_AGENT_FUNCTION_TABLE                   \
    {                                                   \
        [ NONE ] = MQTTAgentCommand_ProcessLoop,        \
        [ PROCESSLOOP ] = MQTTAgentCommand_ProcessLoop, \
        [ PUBLISH ] = MQTTAgentCommand_Publish,         \
        [ SUBSCRIBE ] = MQTTAgentCommand_Subscribe,     \
        [ UNSUBSCRIBE ] = MQTTAgentCommand_Unsubscribe, \
        [ PING ] = MQTTAgentCommand_Ping,               \
        [ CONNECT ] = MQTTAgentCommand_Connect,         \
        [ DISCONNECT ] = MQTTAgentCommand_Disconnect,   \
        [ TERMINATE ] = MQTTAgentCommand_Terminate      \
    }
#endif /* ifndef MQTT_AGENT_FUNCTION_TABLE */

#define MQTT_AGENT_FLAG_COMMAND_COMPLETE    ( ( uint8_t ) 1U << 0 )
#define MQTT_AGENT_ADD_ACKNOWLEDGMENT       ( ( uint8_t ) 1U << 1 )
#define MQTT_AGENT_FLAG_END_LOOP            ( ( uint8_t ) 1U << 2 )


/*-----------------------------------------------------------*/

typedef struct MQTTAgentCommandFuncReturns
{
    uint16_t packetId;
    bool endLoop;
    bool addAcknowledgment;
    bool runProcessLoop;
} MQTTAgentCommandFuncReturns_t;

typedef MQTTStatus_t (* MQTTAgentCommandFunc_t ) ( MQTTAgentContext_t * pMqttAgentContext,
                                                   void * pArgs,
                                                   MQTTAgentCommandFuncReturns_t * pFlags );

/*-----------------------------------------------------------*/

/**
 * @brief Function to execute for a NONE command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pUnusedArg Unused NULL argument.
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_ProcessLoop().
 */
MQTTStatus_t MQTTAgentCommand_ProcessLoop( MQTTAgentContext_t * pMqttAgentContext,
                                           void * pUnusedArg,
                                           MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for a PUBLISH command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pPublishArg Publish information for MQTT_Publish().
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_Publish().
 */
MQTTStatus_t MQTTAgentCommand_Publish( MQTTAgentContext_t * pMqttAgentContext,
                                       void * pPublishArg,
                                       MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for a SUBSCRIBE command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pSubscribeArgs Arguments for MQTT_Subscribe().
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_Subscribe().
 */
MQTTStatus_t MQTTAgentCommand_Subscribe( MQTTAgentContext_t * pMqttAgentContext,
                                         void * pVoidSubscribeArgs,
                                         MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for an UNSUBSCRIBE command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pSubscribeArgs Arguments for MQTT_Unsubscribe().
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_Unsubscribe().
 */
MQTTStatus_t MQTTAgentCommand_Unsubscribe( MQTTAgentContext_t * pMqttAgentContext,
                                           void * pVoidSubscribeArgs,
                                           MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for a CONNECT command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pConnectArgs Arguments for MQTT_Connect().
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_Connect().
 */
MQTTStatus_t MQTTAgentCommand_Connect( MQTTAgentContext_t * pMqttAgentContext,
                                       void * pConnectArgs,
                                       MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for a DISCONNECT command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pUnusedArg Unused NULL argument.
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_Disconnect().
 */
MQTTStatus_t MQTTAgentCommand_Disconnect( MQTTAgentContext_t * pMqttAgentContext,
                                          void * pUnusedArg,
                                          MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for a PING command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pUnusedArg Unused NULL argument.
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return Status code of MQTT_Ping().
 */
MQTTStatus_t MQTTAgentCommand_Ping( MQTTAgentContext_t * pMqttAgentContext,
                                    void * pUnusedArg,
                                    MQTTAgentCommandFuncReturns_t * pReturnFlags );

/**
 * @brief Function to execute for a TERMINATE command.
 *
 * @param[in] pMqttAgentContext MQTT Agent context information.
 * @param[in] pUnusedArg Unused NULL argument.
 * @param[out] pReturnFlags Flags set to indicate actions the MQTT agent should take.
 *
 * @return `MQTTSuccess`.
 */
MQTTStatus_t MQTTAgentCommand_Terminate( MQTTAgentContext_t * pMqttAgentContext,
                                         void * pUnusedArg,
                                         MQTTAgentCommandFuncReturns_t * pReturnFlags );

#endif /* MQTT_AGENT_COMMAND_FUNCTIONS_H */
