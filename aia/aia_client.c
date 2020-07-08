/*
 * Copyright (C) 2019 - 2020 Arm Ltd.  All Rights Reserved.
 * SPDX-License-Identifier: MIT
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "aia_client_priv.h"
#include "aia_platform.h"
#include "jsmn.h"

TaskHandle_t xDemoTaskHandle;

static AIAClient_t AIAClient;

static uint8_t ucAiaRecvMsg[ aiaconfigAIA_MESSAGE_MAX_SIZE ];

static uint8_t ucDecodeTaskTemp[ aiaconfigAIA_MESSAGE_MAX_SIZE ];

static AIAClient_Resequence_t xReseqBuffer;

static AIACryptoKeys_t xKeys = {
        .client_public_key = aiaconfigCLIENT_PUBLIC_KEY,
        .client_private_key = aiaconfigCLIENT_PRIVATE_KEY,
        .peer_public_key = aiaconfigPEER_PUBLIC_KEY,
};

static bool bBufferOverrun = false;
static bool bMicrophoneOpenedDuringOverrun = false;
static bool bSendMicrophoneOpenedEvent = false;
static SemaphoreHandle_t xGenericLock;

static TaskHandle_t xMicrophoneTaskHandle;
static TaskHandle_t xSpeakerTaskHandle;

static BaseType_t prvClientSetState( BaseType_t xState );
static BaseType_t prvClientSetStateFromISR( BaseType_t xState, BaseType_t *pxHigherPriorityTaskWoken );
static BaseType_t prvClientClearState( BaseType_t xState );
static BaseType_t prvClientGetState( BaseType_t xState );
static BaseType_t prvClientWaitForState( BaseType_t xState, BaseType_t xClearOnExit, BaseType_t xWaitForAllStates, TickType_t xTicksToWait );
static BaseType_t prvClientDisconnectFromAIA( void );
static BaseType_t prvClientOpenMicrophone( void );
static BaseType_t prvClientOpenMicrophoneFromISR( BaseType_t * pxHigherPriorityTaskWoken );
static BaseType_t prvClientOpenSpeaker( uint64_t ullOpenOffset );
static BaseType_t prvClientCloseSpeaker( uint64_t ullCloseOffset );
static BaseType_t prvClientSetVolume( AIAClient_SetVolume_t xSetVolume );
static BaseType_t prvClientBufferStateChanged( AIABufferStateChanged_t xBufferStateChanged );
static void prvAIAStreamMicrophoneTask( void * pvParameters );
static void prvAIASpeakerTask( void * pvParameters );

static BaseType_t prvClientSetState( BaseType_t xState )
{
    EventBits_t uxBits;

    uxBits = xEventGroupSetBits( AIAClient.xState, xState );

    /* See the description of xEventGroupSetBits() for why the return value might no have the bits set */
    if( ( uxBits & xState ) == xState )
    {
        return pdPASS;
    }
    else
    {
        return pdFAIL;
    }
}

static BaseType_t prvClientSetStateFromISR( BaseType_t xState, BaseType_t *pxHigherPriorityTaskWoken )
{
    return xEventGroupSetBitsFromISR( AIAClient.xState, xState, pxHigherPriorityTaskWoken );
}

static BaseType_t prvClientClearState( BaseType_t xState )
{
    xEventGroupClearBits( AIAClient.xState, xState );

    /* Clear function does not fail */
    return pdPASS;
}

static BaseType_t prvClientGetState( BaseType_t xState )
{
    if( ( xEventGroupGetBits( AIAClient.xState ) & xState ) != 0 )
    {
        return pdTRUE;
    }
    else
    {
        return pdFALSE;
    }
}

static BaseType_t prvClientWaitForState( BaseType_t xState, BaseType_t xClearOnExit, BaseType_t xWaitForAllStates, TickType_t xTicksToWait )
{
    return xEventGroupWaitBits( AIAClient.xState,
                                xState,
                                xClearOnExit,
                                xWaitForAllStates,
                                xTicksToWait );
}

BaseType_t prvClientPublishMessage( const char * pcTopic, const void * pvData, uint32_t ulLen )
{
    BaseType_t xReturned = pdPASS;
    IotMqttError_t xMqttStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttPublishInfo_t xPublishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;

    xPublishInfo.pTopicName = pcTopic;
    xPublishInfo.topicNameLength = strlen( pcTopic );
    xPublishInfo.qos = IOT_MQTT_QOS_0;
    xPublishInfo.pPayload = pvData;
    xPublishInfo.payloadLength = ulLen;

    xMqttStatus = IotMqtt_Publish( AIAClient.xMqttConnection,
                                   &xPublishInfo,
                                   0,
                                   NULL,
                                   NULL );

    if( xMqttStatus != IOT_MQTT_SUCCESS )
    {
        configPRINTF( ( "Message failed to be published to %s\r\n", pcTopic ) );
        xReturned = pdFAIL;
    }
    else
    {
        configPRINTF_DEBUG( ( "DEBUG: Message is published to %s\r\n", pcTopic ) );
    }

    return xReturned;
}

/* JSON string is not null-terminated, thus must first append a NULL character before printing. */
static void prvPrintJSONString( const char * description, const uint8_t * js, int start, int end )
{
    char * s = ( char * )pvPortMalloc( end - start + 1 );

    if( s != NULL )
    {
        memcpy( s, js + start, end - start );
        s[ end - start ] = 0;
        configPRINTF( ( "%s%s\r\n", description, s ) );
        vPortFree( s );
    }
}

static uint64_t prvConvertJSONLong( const uint8_t * js, int start, int end )
{
    uint64_t num = 0;
    for( ; start < end; start++ )
    {
        num = num * 10 + *( js + start ) - '0';
    }
    return num;
}

static void prvAIAGeneralCallback( void * pvUserData, IotMqttCallbackParam_t * pxPublishParameters )
{
    /* A provisional and very coarse locking block to ensure reentrancy of this callback function. */
    xSemaphoreTake( xGenericLock, portMAX_DELAY );

    int32_t lMsgLen = 0;
    /* This pointer points to the actual message content. */
    const uint8_t * pucMsgContent = NULL;
    const char * pcTopicName = pxPublishParameters->u.message.info.pTopicName;
    uint16_t usTopicNameLength = pxPublishParameters->u.message.info.topicNameLength;

    /* If the message is received on connection topic, no need for decryption. */
    if( memcmp( pcTopicName, AIA_TOPIC_CONNECTION_SER, ( size_t )usTopicNameLength ) != 0 )
    {
        lMsgLen = AIA_CRYPTO_Decrypt( &AIAClient.xCrypto,
                                      ucAiaRecvMsg,
                                      pxPublishParameters->u.message.info.pPayload,
                                      pxPublishParameters->u.message.info.payloadLength );
        if( lMsgLen == eCryptoSequenceNotMatch )
        {
            /* TODO: Should close the connection with a "MESSAGE_TAMPERED" disconnect code. */
            configPRINTF_DEBUG( ( "DEBUG: decrypted sequence number does not match!\r\n" ) );
            goto aia_callback_exit;
        }
        if( lMsgLen == eCryptoFailure )
        {
            configPRINTF( ( "Failed to decrypt received message!\r\n" ) );
            goto aia_callback_exit;
        }
    }
    else
    {
        pucMsgContent = pxPublishParameters->u.message.info.pPayload;
        lMsgLen = pxPublishParameters->u.message.info.payloadLength;
    }

    uint32_t ulSequence = *( uint32_t * )ucAiaRecvMsg;
    configPRINTF_DEBUG( ( "DEBUG: msg length %d seq %u\r\n", lMsgLen, ulSequence ) );

    /* speaker */
    if( memcmp( pcTopicName, AIA_TOPIC_SPEAKER, ( size_t )usTopicNameLength ) == 0 )
    {
        AIAClient_Speaker_t * pxSpeaker = &AIAClient.xSpeaker;
        size_t xBytesRemainedBefore;
        static uint32_t ulNextExpectedSeq;

        if( ulSequence < ulNextExpectedSeq )
        {
            /* Skip the message of a smaller sequence number than expected.
             * This is actually not consistent with AIA spec, which requires to replace
             * the old message if a message with the same sequence number is received
             * twice. In our current implementation using message buffer there's no way
             * to arbitrarily replace data in the buffer.
             * So far observation shows that the only circumstance that duplicate sequence
             * messages are received is when overrun happens. Messages start with the sequence
             * number specified in the event message will be resent by the server. And since
             * the overrun event message won't take effect immediately, we will still receive
             * messages before the server starts resending. Those which are within our
             * resequencing range will be put into the resequencing buffer and pushed into
             * the speaker buffer immediately following the expected message is resent and
             * pushed.
             * Sadly we cannot skip messages until the overrun message is received,because
             * it can also happen that the resent messages are still out of order:)
             * e.g.
             *      Buffer overruns at: 42
             *      Resequencing buffer size: 4
             *                                               these 4 are ignored as they have been sent before
             *                                                                  ^
             *                                                             _____|______
             *                                                             |    |  |  |
             *      Message received on /speaker: 40 41 42 43 44 45 46 47 43 42 44 45 46 47 ...
             *                                           ^                  ^
             *                                           |                  |
             *                            overrun message sent      43 is resent before 42
             */
        }
        else if( ulSequence > ulNextExpectedSeq )
        {
            if( ulSequence > ulNextExpectedSeq + aiaconfigAIA_SPEAKER_RESEQUENCING )
            {
                if( bBufferOverrun == false )
                {
                    configPRINTF_DEBUG( ( "DEBUG: Unexpected sequence %u goes out of range while expecting %u!\r\n", ulSequence, ulNextExpectedSeq ) );
                    prvClientDisconnectFromAIA();
                    /* Signal the demo task. */
                    if( xDemoTaskHandle != NULL )
                    {
                        xTaskNotifyGive( xDemoTaskHandle );
                    }
                }
            }
            else
            {
                uint8_t ucIndex;

                /* Calculate where the message should be put in the resequencing buffer. */
                ucIndex = ( ulSequence - ulNextExpectedSeq - 1 + xReseqBuffer.ucStartIndex ) % aiaconfigAIA_SPEAKER_RESEQUENCING;
                memcpy( xReseqBuffer.xMessage[ ucIndex ].ucBuffer, ucAiaRecvMsg, lMsgLen );
                xReseqBuffer.xMessage[ ucIndex ].ulLen = lMsgLen;
            }
        }
        else
        {
            void * pvData;
            size_t xDataLen;
            bool bContinue;
            AIABufferStateChanged_t xBufferStateChanged;

            if( bBufferOverrun == true )
            {
                bBufferOverrun = false;

                /* If microphone was opened during overrun state, which is the case when media playback
                 * is interrupted by a new user request, drop the messages in the resequence buffer as
                 * messages of the same sequence number but different contents will be sent by the server.
                 */
                if( bMicrophoneOpenedDuringOverrun == true )
                {
                    bMicrophoneOpenedDuringOverrun = false;
                    for( int i = 0; i < aiaconfigAIA_SPEAKER_RESEQUENCING; i++ )
                    {
                       xReseqBuffer.xMessage[ i ].ulLen = 0;
                    }
                }
            }

            pvData = ( void * )ucAiaRecvMsg;
            xDataLen = lMsgLen;

            do
            {
                bContinue = false;

                xBytesRemainedBefore = xStreamBufferBytesAvailable( ( StreamBufferHandle_t )pxSpeaker->xSpeakerBuffer );
                if( xMessageBufferSend( pxSpeaker->xSpeakerBuffer, pvData, xDataLen, pdMS_TO_TICKS( 100 ) ) == 0 )
                {
                    /* According to the spec, overrun event should only be sent when the speaker is opened. If it's
                     * closed, new data should be added to the buffer and old data dropped if the buffer runs out.
                     * It is not handled accordingly here, as it requires extra logic but is only a corner case which
                     * barely happens.
                     */
                    configPRINTF_DEBUG( ( "DEBUG: Speaker buffer overruns!\r\n" ) );
                    bBufferOverrun = true;

                    if( prvClientGetState( AIA_STATE_MICROPHONE_OPENED ) == pdTRUE )
                    {
                        bMicrophoneOpenedDuringOverrun = true;
                    }

                    /* Reset the resequence buffer as the following messages will be resent by AIA. */
                    for( int i = 0; i < aiaconfigAIA_SPEAKER_RESEQUENCING; i++ )
                    {
                        xReseqBuffer.xMessage[ i ].ulLen = 0;
                    }

                    xBufferStateChanged.ulSequence = ulNextExpectedSeq;
                    xBufferStateChanged.pcBufferStateStr = "OVERRUN";

                    /* Only send this event while speaker is open. In case it is not, discard old data. */
                    vTaskSuspendAll();

                    if( prvClientGetState( AIA_STATE_SPEAKER_OPENED ) == pdTRUE )
                    {
                        xTaskResumeAll();
                        prvClientBufferStateChanged( xBufferStateChanged );
                    }
                    else
                    {
                        do
                        {
                            xMessageBufferReceive( pxSpeaker->xSpeakerBuffer, ucDecodeTaskTemp, sizeof( ucDecodeTaskTemp ), 0 );
                        } while( xMessageBufferSend( pxSpeaker->xSpeakerBuffer, pvData, xDataLen, 0 ) == 0 );
                        ulNextExpectedSeq++;
                        xTaskResumeAll();
                    }

                }
                else
                {
                    /* Send the overrun warning only when speaker is still opened and
                     * the buffer goes from a good state to a warning state.
                     */
                    if( prvClientGetState( AIA_STATE_SPEAKER_OPENED ) == pdTRUE &&
                        xBytesRemainedBefore < pxSpeaker->ulSpeakerBufferOverrunWarning &&
                        xStreamBufferBytesAvailable( ( StreamBufferHandle_t )pxSpeaker->xSpeakerBuffer ) >= pxSpeaker->ulSpeakerBufferOverrunWarning )
                    {
                        xBufferStateChanged.ulSequence = ulSequence;
                        xBufferStateChanged.pcBufferStateStr = "OVERRUN_WARNING";
                        prvClientBufferStateChanged( xBufferStateChanged );
                    }

                    /* If the next slot in the resequencing buffer has valid data,
                     * prepare to push it to the speaker buffer.
                     */
                    uint8_t ucNextIndex = xReseqBuffer.ucStartIndex;
                    if( xReseqBuffer.xMessage[ ucNextIndex ].ulLen != 0 )
                    {
                        pvData = xReseqBuffer.xMessage[ ucNextIndex ].ucBuffer;
                        xDataLen = xReseqBuffer.xMessage[ ucNextIndex ].ulLen;
                        xReseqBuffer.xMessage[ ucNextIndex ].ulLen = 0;
                        bContinue = true;
                    }

                    xReseqBuffer.ucStartIndex = ( ucNextIndex + 1 ) % aiaconfigAIA_SPEAKER_RESEQUENCING;
                    ulNextExpectedSeq++;
                }
            } while( bContinue );
        }

        goto aia_callback_exit;
    }

    /* Skip the sequence number if not /connection. */
    if( memcmp( pcTopicName, AIA_TOPIC_CONNECTION_SER, ( size_t )usTopicNameLength ) != 0 )
    {
        pucMsgContent = ucAiaRecvMsg + AIA_MSG_PARAMS_SIZE_SEQ;
        lMsgLen -= AIA_MSG_PARAMS_SIZE_SEQ;
    }

    int32_t lNbTokens;
    jsmn_parser xJSMNParser;
    jsmntok_t pxJSMNTokens[ aiaconfigJSMN_MAX_TOKENS ];
    jsmn_init( &xJSMNParser );
    lNbTokens = jsmn_parse( &xJSMNParser, ( const char * )pucMsgContent, lMsgLen, pxJSMNTokens, aiaconfigJSMN_MAX_TOKENS );
    if( lNbTokens < 0 )
    {
        configPRINTF( ( "Failed to parse received message! Error: %d\r\n", lNbTokens ) );
        goto aia_callback_exit;
    }

    printJSONString_DEBUG( ( "DEBUG: RAW JSON message: ", pucMsgContent, 0, lMsgLen ) );

    /* Handle received message according to the topic. */
    /* connection acknowledge */
    if( memcmp( pcTopicName, AIA_TOPIC_CONNECTION_SER, ( size_t )usTopicNameLength ) == 0 )
    {
        /* Acknowledge and disconnect messages are both published to this topic. */
        if( memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_CONNECTION_NAME ].start, "Acknowledge", strlen( "Acknowledge" ) ) == 0 )
        {
            if( memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_CONNECTION_CODE ].start, "CONNECTION_ESTABLISHED", strlen( "CONNECTION_ESTABLISHED" ) ) == 0 )
            {
                configPRINTF( ( "AIA service is connected!\r\n" ) );
                prvClientSetState( AIA_STATE_CONNECTED );
            }
            else
            {
                prvPrintJSONString( "Failed to connect to AIA service. Code: ", pucMsgContent,
                                    pxJSMNTokens[ AIA_MSGTOKENPOS_CONNECTION_CODE ].start, pxJSMNTokens[ AIA_MSGTOKENPOS_CONNECTION_CODE ].end );
                prvClientSetState( AIA_STATE_CONNECTION_DENIED );
            }
        }
        else if( memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_CONNECTION_NAME ].start, "Disconnect", strlen( "Disconnect" ) ) == 0 )
        {
            /* Ignore the message if the client is not connected. */
            if( prvClientGetState( AIA_STATE_CONNECTED ) == pdTRUE )
            {
                prvPrintJSONString( "Disconnect from AIA service! Code: ", pucMsgContent,
                                    pxJSMNTokens[ AIA_MSGTOKENPOS_DISCONNECTION_CODE ].start, pxJSMNTokens[ AIA_MSGTOKENPOS_DISCONNECTION_CODE ].end );

                prvClientDisconnectFromAIA();
                /* Signal the demo task. */
                if( xDemoTaskHandle != NULL )
                {
                    xTaskNotifyGive( xDemoTaskHandle );
                }
            }
        }
    }
    /* capabilities acknowledge */
    else if( memcmp( pcTopicName, AIA_TOPIC_CAPABILITIES_ACK, ( size_t )usTopicNameLength ) == 0 )
    {
        if( memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_CAPABILITIES_CODE ].start, "CAPABILITIES_ACCEPTED", strlen( "CAPABILITIES_ACCEPTED" ) ) == 0 )
        {
            configPRINTF( ( "AIA has accepted the capabilities!\r\n" ) );
            prvClientSetState( AIA_STATE_CAPABILITIES_ACCEPTED );
        }
        else
        {
            prvPrintJSONString( "AIA has rejected the capabilities! Description: ", pucMsgContent,
                                pxJSMNTokens[ AIA_MSGTOKENPOS_CAPABILITIES_DESCRIPTION ].start, pxJSMNTokens[ AIA_MSGTOKENPOS_CAPABILITIES_DESCRIPTION ].end );
            prvClientSetState( AIA_STATE_CAPABILITIES_REJECTED );
        }
    }
    /* directive
     * One directive message may contain multiple directive objects, e.g.
     *
     * {"directives":[{"header":{"name":"CloseMicrophone","messageId":"42d7b012-e81f-410d-8d74-ec0e96626216"}},
     *                {"header":{"name":"SetAttentionState","messageId":"e7332db8-308b-41d3-b9ae-3f40d164fff9"},"payload":{"state":"THINKING"}}]}
     *
     * {"directives":[{"header":{"name":"SetAttentionState","messageId":"d1c80e21-df0f-4695-b662-14f132c2cfd4"},"payload":{"state":"SPEAKING"}},
     *                {"header":{"name":"OpenSpeaker","messageId":"18600d0d-1464-4936-9960-d7d7cbe398ba"},"payload":{"offset":0}}]}
     */
    else if( memcmp( pcTopicName, AIA_TOPIC_DIRECTIVE, ( size_t )usTopicNameLength ) == 0 )
    {
        uint8_t ucNbDirectives = pxJSMNTokens[ AIA_MSGTOKENPOS_DIRECTIVE_ARRAY ].size;
        /* The token number offset of each directive object. The first one starts at index 3. */
        uint8_t ucDirectiveTokenOffset = 3;
        uint8_t ucDirectiveTokenSize = 0;
        for( uint8_t i = 0; i < ucNbDirectives; i++ )
        {
            const uint8_t * pucDirectiveName = pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_DIRECTIVE_NAME + ucDirectiveTokenOffset ].start;
            if( memcmp( pucDirectiveName, "SetAttentionState", strlen( "SetAttentionState" ) ) == 0 )
            {
                ucDirectiveTokenSize = AIA_MSGTOKENSIZE_SETATTENTIONSTATE;
                /* "offset" field is optional. It needs to be handled before changing the attention state, as it might unblock other tasks immediately. */
                if( AIA_MSGTOKENPOS_SETATTENTIONSTATE_OFFSET + ucDirectiveTokenOffset < lNbTokens &&
                    memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_SETATTENTIONSTATE_OFFSET + ucDirectiveTokenOffset - 1].start, "offset", strlen("offset") ) == 0 )
                {
                    ucDirectiveTokenSize += 2;
                    configPRINTF( ( "We are not handling offset in SetAttentionState yet!!!\r\n" ) );
                }
                const uint8_t * pucState = pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_SETATTENTIONSTATE_STATE + ucDirectiveTokenOffset ].start;
                prvClientClearState( AIA_STATE_ALEXA_MASK );

                if( memcmp( pucState, "IDLE", strlen( "IDLE" ) ) == 0 )
                {
                    configPRINTF( ( "Switching to IDLE state.\r\n" ) );
                    prvClientSetState( AIA_STATE_ALEXA_IDLE );
                    vPlatformTouchButtonEnable();
                    vPlatformLEDOn();
                }
                else if( memcmp( pucState, "THINKING", strlen( "THINKING" ) ) == 0 )
                {
                    configPRINTF( ( "Switching to THINKING state.\r\n" ) );
                    prvClientSetState( AIA_STATE_ALEXA_THINKING );
                }
                else if( memcmp( pucState, "SPEAKING", strlen( "SPEAKING" ) ) == 0 )
                {
                    configPRINTF( ( "Switching to SPEAKING state.\r\n" ) );
                    prvClientSetState( AIA_STATE_ALEXA_SPEAKING );
                }
                else if( memcmp( pucState, "ALERTING", strlen( "ALERTING" ) ) == 0 )
                {
                    configPRINTF( ( "Switching to ALERTING state.\r\n" ) );
                    prvClientSetState( AIA_STATE_ALEXA_ALERTING );
                }
            }
            else if( memcmp( pucDirectiveName, "OpenSpeaker", strlen( "OpenSpeaker" ) ) == 0 )
            {
                ucDirectiveTokenSize = AIA_MSGTOKENSIZE_OPENSPEAKER;
                AIAClient.xSpeaker.ullOpenOffset = prvConvertJSONLong( pucMsgContent,
                                                                       pxJSMNTokens[ AIA_MSGTOKENPOS_OPENSPEAKER_OFFSET + ucDirectiveTokenOffset ].start,
                                                                       pxJSMNTokens[ AIA_MSGTOKENPOS_OPENSPEAKER_OFFSET + ucDirectiveTokenOffset ].end );
                configPRINTF_DEBUG( ( "DEBUG: OpenSpeaker offset is %lu.\r\n", ( uint32_t )AIAClient.xSpeaker.ullOpenOffset ) );
                prvClientSetState( AIA_STATE_OPENSPEAKER_RECEIVED );
            }
            else if( memcmp( pucDirectiveName, "CloseSpeaker", strlen( "CloseSpeaker" ) ) == 0 )
            {
                ucDirectiveTokenSize = AIA_MSGTOKENSIZE_CLOSESPEAKER;
                if( AIA_MSGTOKENPOS_CLOSESPEAKER_OFFSET + ucDirectiveTokenOffset < lNbTokens &&
                    memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_CLOSESPEAKER_OFFSET + ucDirectiveTokenOffset - 1].start, "offset", strlen("offset") ) == 0 ) {
                    AIAClient.xSpeaker.ullCloseOffset = prvConvertJSONLong( pucMsgContent,
                                                                            pxJSMNTokens[ AIA_MSGTOKENPOS_CLOSESPEAKER_OFFSET + ucDirectiveTokenOffset ].start,
                                                                            pxJSMNTokens[ AIA_MSGTOKENPOS_CLOSESPEAKER_OFFSET + ucDirectiveTokenOffset ].end );
                    configPRINTF_DEBUG( ( "DEBUG: CloseSpeaker offset is %lu.\r\n", ( uint32_t )AIAClient.xSpeaker.ullCloseOffset ) );
                    ucDirectiveTokenSize += 4;
                }
                else
                {
                    configPRINTF_DEBUG( ( "DEBUG: CloseSpeaker, no offset\r\n" ) );
                    prvClientSetState( AIA_STATE_CLOSESPEAKERNOOFFSET_RECEIVED );
                }
            }
            else if( memcmp( pucDirectiveName, "OpenMicrophone", strlen( "OpenMicrophone" ) ) == 0 )
            {
                /* We don't handle timeoutInMilliseconds for now and no initiator is received so far. */
                ucDirectiveTokenSize = AIA_MSGTOKENSIZE_OPENMICROPHONE;

                if( AIA_MSGTOKENPOS_INITIATOR + ucDirectiveTokenOffset < lNbTokens &&
                    memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_INITIATOR + ucDirectiveTokenOffset - 1 ].start, "initiator", strlen("initiator") ) == 0 )
                {
                    /* TODO: send the received initiator in the subsequent MicrophoneOpened event and adjust ucDirectiveTokenSize properly. */
                    configPRINTF_DEBUG( ( "DEBUG: Initiator received in OpenMicrophone directive!\r\n" ) );
                }
                else
                {
                    AIAClient.pcInitiatorType = NULL;
                }

                xStreamBufferReset( AIAClient.xMicrophone.xMicBuffer );
                prvClientOpenMicrophone();

                /* Change the blink interval to 200ms in this case. */
                vPlatformLEDBlink( 200 );
            }
            else if( memcmp( pucDirectiveName, "CloseMicrophone", strlen( "CloseMicrophone" ) ) == 0 )
            {
                ucDirectiveTokenSize = AIA_MSGTOKENSIZE_CLOSEMICROPHONE;
                configPRINTF_DEBUG( ( "DEBUG: CloseMicrophone is received.\r\n" ) );
                prvClientClearState( AIA_STATE_MICROPHONE_OPENED );
                vPlatformMicrophoneClose();
                vPlatformLEDOff();
            }
            else if( memcmp( pucDirectiveName, "SetVolume", strlen( "SetVolume" ) ) == 0 )
            {
                ucDirectiveTokenSize = AIA_MSGTOKENSIZE_SETVOLUME;
                AIAClient_SetVolume_t xSetVolume = { 0 };
                xSetVolume.ulVolume = ( uint32_t )prvConvertJSONLong( pucMsgContent,
                                                                      pxJSMNTokens[ AIA_MSGTOKENPOS_SETVOLUME_VOLUME + ucDirectiveTokenOffset ].start,
                                                                      pxJSMNTokens[ AIA_MSGTOKENPOS_SETVOLUME_VOLUME + ucDirectiveTokenOffset ].end );
                if( AIA_MSGTOKENPOS_SETVOLUME_OFFSET + ucDirectiveTokenOffset < lNbTokens &&
                    memcmp( pucMsgContent + pxJSMNTokens[ AIA_MSGTOKENPOS_SETVOLUME_OFFSET + ucDirectiveTokenOffset - 1].start, "offset", strlen("offset") ) == 0 )
                {
                    ucDirectiveTokenSize += 2;
                    xSetVolume.ullOffset = prvConvertJSONLong( pucMsgContent,
                                                               pxJSMNTokens[ AIA_MSGTOKENPOS_SETVOLUME_OFFSET + ucDirectiveTokenOffset ].start,
                                                               pxJSMNTokens[ AIA_MSGTOKENPOS_SETVOLUME_OFFSET + ucDirectiveTokenOffset ].end );
                }
                configPRINTF_DEBUG( ( "DEBUG: SetVolume is received to set volume to %u.\r\n", xSetVolume.ulVolume ) );
                prvClientSetVolume( xSetVolume );
            }
            ucDirectiveTokenOffset += ucDirectiveTokenSize;
        }
    }

aia_callback_exit:
    /* Release the lock */
    xSemaphoreGive( xGenericLock );
}

/* Helper macro after snprintf.
 * @m:  the pointer to the destination string.
 * @b:  number of available bytes left of the destination string.
 * @l:  return value of the last snprintf.
 */
#define SNPRINTF_POST_PROCESS( m, b, l )    ({ if( l < 0 ) return pdFAIL; m += l; b -= l; })

static BaseType_t prvGenerateCapabilitiesJSON( char * pcCapabilities )
{
    configASSERT( AIAClient.xSpeaker.ulSpeakerBufferSize != 0 );
    configASSERT( AIAClient.xSpeaker.ulSpeakerBufferOverrunWarning != 0 );
    configASSERT( AIAClient.xSpeaker.ulSpeakerBufferUnderrunWarning != 0 );
    configASSERT( AIAClient.xSpeaker.ulDecoderBitrate != 0 );
    configASSERT( AIAClient.xSpeaker.ucChannels != 0 );

    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;
    xLength = snprintf( pcCapabilities, xBytesLeft,
                        "{"                                                                                     \
                            "\"header\":{"                                                                      \
                                "\"name\":\"Publish\","                                                         \
                                "\"messageId\":\"%s_Capabilities\""                                             \
                            "},"                                                                                \
                            "\"payload\":{"                                                                     \
                                "\"capabilities\":["                                                            \
                                    "{"                                                                         \
                                        "\"type\":\"AisInterface\","                                            \
                                        "\"interface\":\"Speaker\","                                            \
                                        "\"version\":\"1.0\","                                                  \
                                        "\"configurations\":{"                                                  \
                                            "\"audioBuffer\":{"                                                 \
                                                "\"sizeInBytes\":%lu,"                                          \
                                                "\"reporting\":{"                                               \
                                                    "\"overrunWarningThreshold\":%lu,"                          \
                                                    "\"underrunWarningThreshold\":%lu"                          \
                                                "}"                                                             \
                                            "},"                                                                \
                                            "\"audioDecoder\":{"                                                \
                                                "\"format\":\"OPUS\","                                          \
                                                "\"bitrate\":{"                                                 \
                                                    "\"type\":\"CONSTANT\","                                    \
                                                    "\"bitsPerSecond\":%lu"                                     \
                                                "},"                                                            \
                                                "\"numberOfChannels\":%u"                                       \
                                            "}"                                                                 \
                                        "}"                                                                     \
                                    "},"                                                                        \
                                    "{"                                                                         \
                                        "\"type\":\"AisInterface\","                                            \
                                        "\"interface\":\"Microphone\","                                         \
                                        "\"version\":\"1.0\","                                                  \
                                        "\"configurations\":{"                                                  \
                                            "\"audioEncoder\":{"                                                \
                                                "\"format\":\"AUDIO_L16_RATE_16000_CHANNELS_1\""                \
                                            "}"                                                                 \
                                        "}"                                                                     \
                                    "},"                                                                        \
                                    "{"                                                                         \
                                        "\"type\":\"AisInterface\","                                            \
                                        "\"interface\":\"System\","                                             \
                                        "\"version\":\"1.0\","                                                  \
                                        "\"configurations\":{"                                                  \
                                            "\"mqtt\":{"                                                        \
                                                "\"message\":{"                                                 \
                                                    "\"maxSizeInBytes\":%lu"                                    \
                                                "}"                                                             \
                                            "},"                                                                \
                                            "\"firmwareVersion\":\"42\","                                       \
                                            "\"locale\":\"en-US\""                                              \
                                        "}"                                                                     \
                                    "}"                                                                         \
                                "]"                                                                             \
                            "}"                                                                                 \
                        "}",
                        clientcredentialIOT_THING_NAME,
                        AIAClient.xSpeaker.ulSpeakerBufferSize,
                        AIAClient.xSpeaker.ulSpeakerBufferOverrunWarning,
                        AIAClient.xSpeaker.ulSpeakerBufferUnderrunWarning,
                        AIAClient.xSpeaker.ulDecoderBitrate,
                        AIAClient.xSpeaker.ucChannels,
                        aiaconfigAIA_MESSAGE_MAX_SIZE );
    SNPRINTF_POST_PROCESS( pcCapabilities, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateMicrophoneOpenedJSON( char * pcEventMessage, uint32_t ulMessageId )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"MicrophoneOpened\","    \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"profile\":\"%s\",",
                        ulMessageId,
                        AIAClient.pcASRProfile );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );

    if( AIAClient.pcInitiatorType != NULL )
    {
        xLength = snprintf( pcEventMessage, xBytesLeft,
                            "\"initiator\":{"           \
                                "\"type\":\"%s\"",
                            AIAClient.pcInitiatorType );
        SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );

        if( AIAClient.pcMicrophoneToken != NULL ||
                strncmp( AIAClient.pcInitiatorType, "WAKEWORD", strlen("WAKEWORD") ) == 0 )
        {
            xLength = snprintf( pcEventMessage, xBytesLeft,
                                ",\"payload\":{" );
            SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );

            if( AIAClient.pcMicrophoneToken != NULL )
            {
                /* TODO: not sure this field is still valid so simply give a debugging message here. */
                configPRINTF_DEBUG( ( "DEBUG: Microphone token is present!\r\n" ) );
            }
            if( strncmp( AIAClient.pcInitiatorType, "WAKEWORD", strlen("WAKEWORD") ) == 0 )
            {
                xLength = snprintf( pcEventMessage, xBytesLeft,
                                    "\"wakeWord\":\"%s\","          \
                                    "\"wakeWordIndices\":{"         \
                                        "\"beginOffset\":%lu,"      \
                                        "\"endOffset\":%lu"         \
                                    "}",
                                    AIAClient.xWakeword.pcWakeWordString,
                                    ( uint32_t )AIAClient.xWakeword.ullWakeWordBegin,
                                    ( uint32_t )AIAClient.xWakeword.ullWakeWordEnd );
                SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
            }
            xLength = snprintf( pcEventMessage, xBytesLeft, "}" );
            SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
        }
        xLength = snprintf( pcEventMessage, xBytesLeft, "}," );
        SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    }

    xLength = snprintf( pcEventMessage, xBytesLeft,
                                        "\"offset\":%lu"    \
                                    "}"                     \
                                "}"                         \
                            "]"                             \
                        "}",
                        ( uint32_t )AIAClient.xMicrophone.ullMicrophoneOffset );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );

    return pdPASS;
}

static BaseType_t prvGenerateSynchronizeStateJSON( char * pcEventMessage, uint32_t ulMessageId )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"SynchronizeState\","    \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{",
                        ulMessageId );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    if( AIAClient.xSpeaker.xIsSupported == pdTRUE )
    {
        if( AIAClient.xDeviceAlerts.xIsSupported == pdTRUE )
        {
            xLength = snprintf( pcEventMessage, xBytesLeft,
                                "\"speaker\":{"         \
                                    "\"volume\":%lu"   \
                                "},",
                                AIAClient.xSpeaker.ulVolume );
        }
        else
        {
            xLength = snprintf( pcEventMessage, xBytesLeft,
                                "\"speaker\":{"         \
                                    "\"volume\":%lu"   \
                                "}",
                                AIAClient.xSpeaker.ulVolume );
        }
        SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    }
    if( AIAClient.xDeviceAlerts.xIsSupported == pdTRUE )
    {
        xLength = snprintf( pcEventMessage, xBytesLeft,
                            "\"alerts\":{"              \
                                "\"allAlerts\":[\"%s\"",
                            AIAClient.xDeviceAlerts.ppcAlerts[0] );
        SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );

        for( int i = 1; i < AIAClient.xDeviceAlerts.ulAlertsNum; i++ )
        {
            xLength = snprintf( pcEventMessage, xBytesLeft, ",\"%s\"", AIAClient.xDeviceAlerts.ppcAlerts[i] );
            SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
        }
        xLength = snprintf( pcEventMessage, xBytesLeft,
                                "]" \
                            "}" );
        SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    }
    xLength = snprintf( pcEventMessage, xBytesLeft,
                                    "}"     \
                                "}"         \
                            "]"             \
                        "}" );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateMicrophoneClosedJSON( char * pcEventMessage, uint32_t ulMessageId )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"MicrophoneClosed\","    \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"offset\":%lu"                    \
                                    "}"                                     \
                                "}"                                         \
                            "]"                                             \
                        "}",
                        ulMessageId, ( uint32_t )AIAClient.xMicrophone.ullMicrophoneOffset );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateSpeakerOpenedJSON( char * pcEventMessage, uint32_t ulMessageId, uint64_t ullOffset )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"SpeakerOpened\","       \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"offset\":%lu"                    \
                                    "}"                                     \
                                "}"                                         \
                            "]"                                             \
                        "}",
                        ulMessageId, ( uint32_t )ullOffset );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateSpeakerClosedJSON( char * pcEventMessage, uint32_t ulMessageId, uint64_t ullOffset )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"SpeakerClosed\","       \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"offset\":%lu"                    \
                                    "}"                                     \
                                "}"                                         \
                            "]"                                             \
                        "}",                                                \
                        ulMessageId, ( uint32_t )ullOffset );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateSpeakerMarkerEncounteredJSON( char * pcEventMessage, uint32_t ulMessageId, uint32_t ulMarker )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                         \
                            "\"events\":["                                          \
                                "{"                                                 \
                                    "\"header\":{"                                  \
                                        "\"name\":\"SpeakerMarkerEncountered\","    \
                                        "\"messageId\":\"%lu\""                     \
                                    "},"                                            \
                                    "\"payload\":{"                                 \
                                        "\"marker\":%lu"                            \
                                    "}"                                             \
                                "}"                                                 \
                            "]"                                                     \
                        "}",                                                        \
                        ulMessageId, ulMarker );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateVolumeChangedJSON( char * pcEventMessage, uint32_t ulMessageId, uint32_t ulVolume )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"VolumeChanged\","       \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"volume\":%lu"                    \
                                    "}"                                     \
                                "}"                                         \
                            "]"                                             \
                        "}",                                                \
                        ulMessageId, ulVolume );

    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateBufferStateChangedJSON( char * pcEventMessage, uint32_t ulMessageId, AIABufferStateChanged_t *xBufferStateChanged )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"BufferStateChanged\","  \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"message\":{"                     \
                                         "\"topic\":\"%s\","                \
                                         "\"sequenceNumber\":%lu"           \
                                         "},"                               \
                                        "\"state\":\"%s\""                  \
                                    "}"                                     \
                                "}"                                         \
                            "]"                                             \
                        "}",                                                \
                        ulMessageId,
                        "speaker",
                        xBufferStateChanged->ulSequence,
                        xBufferStateChanged->pcBufferStateStr );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateButtonCommandJSON( char * pcEventMessage, char * command, uint32_t ulMessageId )
{
    int xLength;
    size_t xBytesLeft = AIA_EVENT_MESSAGE_MAX_SIZE;

    xLength = snprintf( pcEventMessage, xBytesLeft,
                        "{"                                                 \
                            "\"events\":["                                  \
                                "{"                                         \
                                    "\"header\":{"                          \
                                        "\"name\":\"ButtonCommandIssued\"," \
                                        "\"messageId\":\"%lu\""             \
                                    "},"                                    \
                                    "\"payload\":{"                         \
                                        "\"command\":\"%s\""                \
                                    "}"                                     \
                                "}"                                         \
                            "]"                                             \
                        "}",                                                \
                        ulMessageId, command );
    SNPRINTF_POST_PROCESS( pcEventMessage, xBytesLeft, xLength );
    return pdPASS;
}

static BaseType_t prvGenerateStopPlayingJSON( char * pcEventMessage, uint32_t ulMessageId )
{
    return prvGenerateButtonCommandJSON( pcEventMessage, "STOP", ulMessageId );
}

#define SEND_EVENT_GOTO_FAIL( expr, str )             \
        { if( ( expr ) == true ) {                    \
            configPRINTF( ( str ) );                  \
            xReturned = pdFAIL;                       \
            goto send_event_exit; } }

static BaseType_t prvClientSendEvent( AIAEvent_t event_type, void * parameters )
{
    static uint32_t ulEventSequence = 0;
    static uint32_t ulMessageId = 0;
    BaseType_t xReturned = pdPASS;
    char * pcBlob = NULL;
    char * pcEventMessage;
    char * pcEncryptedMessage = NULL;
    int32_t lEncryptedMessageLength;
    uint32_t ulSeq = 0;
    uint32_t ulId = 0;

    /* Allocate a buffer of EVENT_MESSAGE_MAX_SIZE to hold the event message. */
    pcBlob = ( char * )pvPortMalloc( AIA_EVENT_MESSAGE_MAX_SIZE );
    SEND_EVENT_GOTO_FAIL( pcBlob == NULL, "Failed to allocate memory for event message!\r\n");

    /* Leave enough space to hold the squence number. */
    pcEventMessage = pcBlob + AIA_MSG_PARAMS_SIZE_SEQ;

    /* Allocate a buffer to hold the encrypted event message. */
    pcEncryptedMessage = ( char * )pvPortMalloc( sizeof( AIAMessage_t ) + AIA_MSG_PARAMS_SIZE_SEQ + AIA_EVENT_MESSAGE_MAX_SIZE );
    SEND_EVENT_GOTO_FAIL( pcEncryptedMessage == NULL, "Failed to allocate memory for encrypted event message!\r\n" );

    /* Lock before updating sequence number and message Id for reentrancy. */
    vTaskSuspendAll();
    ulSeq = ulEventSequence++;
    /* Simply use an increasing number for message Id for now. */
    ulId = ulMessageId++;
    xTaskResumeAll();

    switch( event_type )
    {
        case aiaEventMicrophoneOpened:
            SEND_EVENT_GOTO_FAIL( prvGenerateMicrophoneOpenedJSON( pcEventMessage, ulId ) == pdFAIL,
                                  "Failed to generate MicrophoneOpened message" );
            break;
        case aiaEventSynchronizeState:
            SEND_EVENT_GOTO_FAIL( prvGenerateSynchronizeStateJSON( pcEventMessage, ulId ) == pdFAIL,
                                  "Failed to generate SynchronizeState message" );
            break;
        case aiaEventMicrophoneClosed:
            SEND_EVENT_GOTO_FAIL( prvGenerateMicrophoneClosedJSON( pcEventMessage, ulId ) == pdFAIL,
                                  "Failed to generate MicrophoneClosed message" );
            break;
        case aiaEventSpeakerOpened:
            SEND_EVENT_GOTO_FAIL( prvGenerateSpeakerOpenedJSON( pcEventMessage, ulId, *( uint64_t * )parameters ) == pdFAIL,
                                  "Failed to generate SpeakerOpened message" );
            break;
        case aiaEventSpeakerClosed:
            SEND_EVENT_GOTO_FAIL( prvGenerateSpeakerClosedJSON( pcEventMessage, ulId, *( uint64_t * )parameters ) == pdFAIL,
                                  "Failed to generate SpeakerClosed message" );
            break;
        case aiaEventSpeakerMarkerEncountered:
            SEND_EVENT_GOTO_FAIL( prvGenerateSpeakerMarkerEncounteredJSON( pcEventMessage, ulId, *( uint32_t * )parameters ) == pdFAIL,
                                  "Failed to generate SpeakerMarkerEncountered message" );
            break;
        case aiaEventBufferStateChanged:
            SEND_EVENT_GOTO_FAIL( prvGenerateBufferStateChangedJSON( pcEventMessage, ulId, ( AIABufferStateChanged_t * )parameters ) == pdFAIL,
                                  "Failed to generate BufferStateChanged message" );
            break;
        case aiaEventVolumeChanged:
            SEND_EVENT_GOTO_FAIL( prvGenerateVolumeChangedJSON( pcEventMessage, ulId, *( uint32_t * )parameters ) == pdFAIL,
                                  "Failed to generate VolumeChanged message" );
            break;
        case aiaEventStopPlaying:
            SEND_EVENT_GOTO_FAIL( prvGenerateStopPlayingJSON( pcEventMessage, ulId ) == pdFAIL,
                                  "Failed to generate StopPlaying message" );
            break;
        default:
            SEND_EVENT_GOTO_FAIL( true ,"Unsupported event type!\r\n" );
    }

    lEncryptedMessageLength = AIA_CRYPTO_Encrypt( &AIAClient.xCrypto, pcEncryptedMessage, pcEventMessage,
                                            strlen(pcEventMessage), ulSeq );
    SEND_EVENT_GOTO_FAIL( lEncryptedMessageLength < 0, "Failed to encrypt the event message");

    configPRINTF_DEBUG( ( "DEBUG: Sending event message %u: %s\r\n", ulSeq, pcEventMessage ) );
    xReturned = prvClientPublishMessage( AIA_TOPIC_EVENT, pcEncryptedMessage, lEncryptedMessageLength );

send_event_exit:
    vPortFree( pcBlob );
    vPortFree( pcEncryptedMessage );
    return xReturned;
}

static BaseType_t prvClientSubscribe( const char * pcTopic )
{
    BaseType_t xReturned;
    IotMqttError_t xMqttStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttSubscription_t xSubscription = IOT_MQTT_SUBSCRIPTION_INITIALIZER;

    xSubscription.pTopicFilter = pcTopic;
    xSubscription.topicFilterLength = strlen( pcTopic );
    xSubscription.qos = IOT_MQTT_QOS_0;
    xSubscription.callback.pCallbackContext = NULL;
    xSubscription.callback.function = prvAIAGeneralCallback;    /* Subscribe to the topic with the general callback. */

    xMqttStatus = IotMqtt_TimedSubscribe( AIAClient.xMqttConnection,
                                          &xSubscription,
                                          1,
                                          0,
                                          aiaconfigAIA_DEFAULT_TIMEOUT );

    if( xMqttStatus == IOT_MQTT_SUCCESS )
    {
        configPRINTF( ( "AIAClient subscribed to %s\r\n", pcTopic ) );
        xReturned = pdPASS;
    }
    else
    {
        configPRINTF( ( "AIAClient failed to subscribe to %s\r\n", pcTopic ) );
        xReturned = pdFAIL;
    }

    return xReturned;
}

static BaseType_t prvClientConnectToAIA( void )
{
    BaseType_t xReturned;
    BaseType_t xState;

    configPRINTF( ( "Connecting to AIA Service...\r\n" ) );
    xReturned = prvClientPublishMessage( AIA_TOPIC_CONNECTION_CLI, AIA_MSG_CONNECT, strlen(AIA_MSG_CONNECT) );

    if( xReturned == pdPASS )
    {
        xState = prvClientWaitForState( AIA_STATE_CONNECTED | AIA_STATE_CONNECTION_DENIED,
                                        pdFALSE,
                                        pdFALSE,
                                        aiaconfigAIA_DEFAULT_TIMEOUT );
        if( ( xState & AIA_STATE_CONNECTED ) != 0 )
        {
            xReturned = pdPASS;
        }
        else if( ( xState & AIA_STATE_CONNECTION_DENIED ) != 0 )
        {
            xReturned = pdFAIL;
        }
        else
        {
            configPRINTF( ( "Connecting to AIA service times out!\r\n" ) );
            xReturned = pdFAIL;
        }
    }

    return xReturned;
}

static BaseType_t prvClientDisconnectFromAIA( void )
{
    BaseType_t xReturned;

    prvClientClearState( AIA_STATE_CONNECTED );
    configPRINTF( ( "Disconnecting from AIA service...\r\n" ) );
    xReturned = prvClientPublishMessage( AIA_TOPIC_CONNECTION_CLI, AIA_MSG_DISCONNECT, strlen( AIA_MSG_DISCONNECT ) );

    /* No wait for the response from the server for now. */
    return xReturned;
}

static BaseType_t prvClientReconnectToAIA( void )
{
    BaseType_t retry;
    BaseType_t interval = aiaconfigAIA_RECONNECT_INTERVAL;

    if( prvClientDisconnectFromAIA() != pdPASS )
    {
        return pdFAIL;
    }

    for( retry = 1; retry <= aiaconfigAIA_RECONNECT_RETRY; retry++ )
    {
        configPRINTF( ( "Attempt %u to reconnect to AIA...\r\n", retry ) ) ;
        vTaskDelay( interval );
        if( prvClientConnectToAIA() == pdPASS )
        {
            return pdPASS;
        }
        interval <<= 1;
    }

    configPRINTF( ( "Failed to reconnect to AIA!\r\n" ) );
    return pdFAIL;
}

static BaseType_t prvClientPublishCapabilities( void )
{
    BaseType_t xReturned;
    BaseType_t xState;
    static uint32_t ulCapabilitiesSequence = 0;
    char * pcBlob = NULL;
    char * pcCapabilitiesMessage;
    char * pcEncryptedMessage = NULL;
    int32_t lEncryptedMessageLength;

    /* Subscribe to capabilities/acknowledge topic. */
    xReturned = prvClientSubscribe( AIA_TOPIC_CAPABILITIES_ACK );
    if( xReturned == pdFAIL )
    {
        return pdFAIL;
    }

    /* Subscribe to directive topic. */
    xReturned = prvClientSubscribe( AIA_TOPIC_DIRECTIVE );
    if( xReturned == pdFAIL )
    {
        return pdFAIL;
    }

    xReturned = prvClientSubscribe( AIA_TOPIC_SPEAKER );
    if( xReturned == pdFAIL )
    {
        return pdFAIL;
    }

    pcBlob = ( char * )pvPortMalloc( AIA_EVENT_MESSAGE_MAX_SIZE );
    if( pcBlob == NULL )
    {
        configPRINTF( ( "Failed to allocate memory for capabilities message!\r\n" ) );
        return pdFAIL;
    }

    pcCapabilitiesMessage = pcBlob + AIA_MSG_PARAMS_SIZE_SEQ;
    prvGenerateCapabilitiesJSON( pcCapabilitiesMessage );

    /* Allocate a buffer to hold the encrypted message. */
    pcEncryptedMessage = ( char * )pvPortMalloc( sizeof( AIAMessage_t ) + AIA_MSG_PARAMS_SIZE_SEQ + AIA_EVENT_MESSAGE_MAX_SIZE );
    if( pcEncryptedMessage == NULL )
    {
        configPRINTF( ( "Failed to allocate memory for encrypted event message!\r\n" ) );
        vPortFree( pcBlob );
        return pdFAIL;
    }

    lEncryptedMessageLength = AIA_CRYPTO_Encrypt( &AIAClient.xCrypto,
                                                  pcEncryptedMessage,
                                                  pcCapabilitiesMessage,
                                                  strlen( pcCapabilitiesMessage ),
                                                  ulCapabilitiesSequence );
    vPortFree( pcBlob );
    if( lEncryptedMessageLength < 0 )
    {
        vPortFree( pcEncryptedMessage );
        return pdFAIL;
    }

    xReturned = prvClientPublishMessage( AIA_TOPIC_CAPABILITIES_PUB, pcEncryptedMessage, lEncryptedMessageLength );
    vPortFree( pcEncryptedMessage );

    if( xReturned == pdPASS )
    {
        ulCapabilitiesSequence++;
        xState = prvClientWaitForState( AIA_STATE_CAPABILITIES_ACCEPTED | AIA_STATE_CAPABILITIES_REJECTED,
                                        pdFALSE,
                                        pdFALSE,
                                        aiaconfigAIA_DEFAULT_TIMEOUT );
        if( ( xState & AIA_STATE_CAPABILITIES_ACCEPTED ) != 0 )
        {
            xReturned = pdPASS;
        }
        else if( ( xState & AIA_STATE_CAPABILITIES_REJECTED ) != 0 )
        {
            xReturned = pdFAIL;
        }
        else
        {
            configPRINTF( (" Publishing capabilities to AIA service times out!\r\n" ) );
            xReturned = pdFAIL;
        }
    }

    return xReturned;
}

static BaseType_t prvClientSynchronizeState( void )
{
    return prvClientSendEvent( aiaEventSynchronizeState, NULL );
}

static BaseType_t prvClientSetVolume( AIAClient_SetVolume_t xSetVolume )
{
    AIAClient.xSpeaker.ulVolume = xSetVolume.ulVolume;
    return prvClientSendEvent( aiaEventVolumeChanged, &xSetVolume.ulVolume );
}

static BaseType_t prvClientOpenMicrophone( void )
{
    BaseType_t xReturned;

    xReturned = prvClientSetState( AIA_STATE_MICROPHONE_OPENED );
    if( xReturned == pdPASS )
    {
        vTaskSuspendAll();
        if( bBufferOverrun == true )
        {
            bMicrophoneOpenedDuringOverrun = true;
        }
        xTaskResumeAll();

        bSendMicrophoneOpenedEvent = true;
    }

    vPlatformLEDBlink( 500 );
    vPlatformMicrophoneOpen();

    return xReturned;
}

static BaseType_t prvClientOpenMicrophoneFromISR( BaseType_t * pxHigherPriorityTaskWoken )
{
    BaseType_t xReturned;

    xReturned = prvClientSetStateFromISR( AIA_STATE_MICROPHONE_OPENED, pxHigherPriorityTaskWoken );
    if( xReturned == pdPASS )
    {
        if( bBufferOverrun == true )
        {
            bMicrophoneOpenedDuringOverrun = true;
        }

        bSendMicrophoneOpenedEvent = true;
    }

    vPlatformLEDBlink( 500 );
    vPlatformMicrophoneOpen();

    return xReturned;
}

static BaseType_t prvClientOpenSpeaker( uint64_t ullOpenOffset )
{
    BaseType_t xReturned;

    xStreamBufferReset( AIAClient.xSpeaker.xDecodeBuffer );
    vPlatformSpeakerOpen();

    xReturned = prvClientSendEvent( aiaEventSpeakerOpened, &ullOpenOffset );
    if( xReturned == pdPASS )
    {
        prvClientSetState( AIA_STATE_SPEAKER_OPENED );
    }

    return xReturned;
}

static BaseType_t prvClientCloseSpeaker( uint64_t ullCloseOffset )
{
    BaseType_t xReturned;

    vPlatformSpeakerClose();
    prvClientClearState( AIA_STATE_SPEAKER_OPENED );
    xReturned = prvClientSendEvent( aiaEventSpeakerClosed, &ullCloseOffset );

    return xReturned;
}

static BaseType_t prvClientSendMarker( uint32_t ulMarker )
{
    return prvClientSendEvent( aiaEventSpeakerMarkerEncountered, &ulMarker );
}

static BaseType_t prvClientBufferStateChanged( AIABufferStateChanged_t xBufferStateChanged )
{
    return prvClientSendEvent( aiaEventBufferStateChanged, &xBufferStateChanged );
}

#define STREAM_TASK_GOTO_FAIL( expr, str )            \
        { if( ( expr ) == true ) {                    \
            configPRINTF( ( str ) );                  \
            goto stream_task_exit; } }

static void prvAIAStreamMicrophoneTask( void * pvParameters )
{
    BaseType_t xReturned;
    char * pcBlob = NULL;
    AIABinaryAudioStream_t * xAudioStream;
    AIAClient_Microphone_t * pxMicrophone;
    TimeOut_t xTimeOut;
    size_t xBytesReceived;
    TickType_t xTicksToWait;
    char * pcEncryptedMessage = NULL;
    int32_t lEncryptedMessageLength;

    /* Allocate memory for receiving data from microphone buffer */
    pcBlob = ( char * )pvPortMalloc( AIA_MSG_PARAMS_SIZE_SEQ + sizeof( AIABinaryAudioStream_t ) );
    STREAM_TASK_GOTO_FAIL( pcBlob == NULL, "Failed to allocate memory for audio stream!\r\n" );

    /* Allocate a buffer to hold the encrypted message. */
    pcEncryptedMessage = ( char * )pvPortMalloc( sizeof( AIAMessage_t ) + AIA_MSG_PARAMS_SIZE_SEQ + sizeof( AIABinaryAudioStream_t ) );
    STREAM_TASK_GOTO_FAIL( pcEncryptedMessage == NULL, "Failed to allocate memory for encrypted event message!\r\n" );

    xAudioStream = ( AIABinaryAudioStream_t * )( pcBlob + AIA_MSG_PARAMS_SIZE_SEQ );
    memset( xAudioStream, 0x0, sizeof(AIABinaryAudioStream_t) );

    pxMicrophone = &AIAClient.xMicrophone;

    for( ;; )
    {
        prvClientWaitForState( AIA_STATE_MICROPHONE_OPENED, pdFALSE, pdFALSE, portMAX_DELAY );
        if( bSendMicrophoneOpenedEvent == true )
        {
            bSendMicrophoneOpenedEvent = false;
            xReturned = prvClientSendEvent( aiaEventMicrophoneOpened, NULL );
            STREAM_TASK_GOTO_FAIL( xReturned != pdPASS, "" );
        }

        xBytesReceived = 0;

        /* Set a total timeout of the duration of one audio message plus extra 50ms */
        xTicksToWait = pdMS_TO_TICKS( aiaconfigAIA_AUDIO_DATA_SIZE / ( aiaconfigCLIENT_MICROPHONE_RAW_SAMPLE_RATE * aiaconfigCLIENT_MICROPHONE_RAW_SAMPLE_RESOLUTION / 8 / 1000 ) + 50 );
        vTaskSetTimeOutState( &xTimeOut );

        while( xBytesReceived < aiaconfigAIA_AUDIO_DATA_SIZE && xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) != pdTRUE )
        {
            xBytesReceived += xStreamBufferReceive( pxMicrophone->xMicBuffer,
                                                    xAudioStream->ucAudio + xBytesReceived,
                                                    aiaconfigAIA_AUDIO_DATA_SIZE - xBytesReceived,
                                                    xTicksToWait );
        }

        if( xBytesReceived != 0 )
        {
            xAudioStream->ullOffset = pxMicrophone->ullMicrophoneOffset;
            xAudioStream->xHeader.ulLength = xBytesReceived + sizeof( xAudioStream->ullOffset );

            lEncryptedMessageLength = AIA_CRYPTO_Encrypt( &AIAClient.xCrypto,
                                                          pcEncryptedMessage,
                                                          xAudioStream,
                                                          xAudioStream->xHeader.ulLength + sizeof( AIABinaryHeader_t ),
                                                          pxMicrophone->ulMicrophoneSequence );
            STREAM_TASK_GOTO_FAIL( lEncryptedMessageLength < 0, "Failed to encrypt message!\r\n" );

            /* Only publish the message if CloseMicrophone is not received yet. */
            if( prvClientGetState( AIA_STATE_MICROPHONE_OPENED ) == pdTRUE )
            {
                xReturned = prvClientPublishMessage( AIA_TOPIC_MICROPHONE, pcEncryptedMessage, lEncryptedMessageLength );
                STREAM_TASK_GOTO_FAIL( xReturned != pdPASS, "" );

                pxMicrophone->ulMicrophoneSequence++;
                pxMicrophone->ullMicrophoneOffset += xBytesReceived;
            }
        }
    }

stream_task_exit:
    if( pcBlob != NULL )
    {
        vPortFree( pcBlob );
    }

    if( pcEncryptedMessage != NULL )
    {
        vPortFree( pcEncryptedMessage );
    }

    /* Signal the demo task. */
    if( xDemoTaskHandle != NULL )
    {
        xTaskNotifyGive( xDemoTaskHandle );
    }
    vTaskDelete( NULL );
}

static void prvAIASpeakerTask( void * pvParameters )
{
    size_t xMsgLen;
    uint32_t ulSeq = 0;
    AIAClient_Speaker_t * pxSpeaker = &AIAClient.xSpeaker;
    AIABinaryHeader_t * pxBinaryHeader;
    uint64_t ullOffset;
    uint8_t * pucMsg;
    int16_t sDecodeTemp[ AIA_SPEAKER_RAW_FRAME_SAMPLES ];
    size_t xBytesRemainedBefore, xBytesRemained;
    AIABufferStateChanged_t xBufferStateChanged;

    for( ; ; )
    {
        if( prvClientGetState( AIA_STATE_SPEAKER_OPENED ) != pdTRUE )
        {
            prvClientWaitForState( AIA_STATE_OPENSPEAKER_RECEIVED, pdFALSE, pdFALSE, portMAX_DELAY );
        }

        xBytesRemainedBefore = xStreamBufferBytesAvailable( ( StreamBufferHandle_t )pxSpeaker->xSpeakerBuffer );
        if( xBytesRemainedBefore == 0  && prvClientGetState( AIA_STATE_OPENSPEAKER_RECEIVED ) != pdTRUE )
        {
            xBufferStateChanged.ulSequence = ulSeq + 1;
            xBufferStateChanged.pcBufferStateStr = "UNDERRUN";
            prvClientBufferStateChanged( xBufferStateChanged );
        }

        xMsgLen = xMessageBufferReceive( pxSpeaker->xSpeakerBuffer, ucDecodeTaskTemp, sizeof( ucDecodeTaskTemp ), pdMS_TO_TICKS( 2000 ) );

        if( xMsgLen == 0 )
        {
            configPRINTF_DEBUG( ( "DEBUG: No data in the speaker buffer!\r\n" ) );
            if( prvClientGetState( AIA_STATE_CLOSESPEAKERNOOFFSET_RECEIVED ) == pdTRUE )
            {
                /* In case that a CloseSpeaker directive with no offset is received, close the speaker
                 * at the current output offset.
                 */
                prvClientClearState( AIA_STATE_CLOSESPEAKERNOOFFSET_RECEIVED );
                pxSpeaker->ullCloseOffset = pxSpeaker->ullOutputOffset;

                prvClientCloseSpeaker( pxSpeaker->ullOutputOffset );
            }
            continue;
        }

        ulSeq = *( uint32_t * )ucDecodeTaskTemp;
        xBytesRemained = xStreamBufferBytesAvailable( ( StreamBufferHandle_t )pxSpeaker->xSpeakerBuffer );

        pucMsg = ucDecodeTaskTemp + sizeof( ulSeq );
        xMsgLen -= sizeof( ulSeq );
        while( xMsgLen )
        {
            pxBinaryHeader = ( AIABinaryHeader_t * )pucMsg;
            pucMsg += sizeof( AIABinaryHeader_t );
            if( pxBinaryHeader->ucType == 0 )
            {
                uint32_t ulLenAudio = pxBinaryHeader->ulLength - sizeof( ullOffset );
                uint32_t ulCount = pxBinaryHeader->ucCount + 1;
                /* Assert if the chunk size received is not equal to the frame size we expect. */
                configASSERT( ulLenAudio / ulCount == AIA_SPEAKER_DECODER_FRAME_SIZE );
                ullOffset = *( uint64_t * )pucMsg;
                if( ullOffset >= pxSpeaker->ullOpenOffset )
                {
                    if( prvClientGetState( AIA_STATE_OPENSPEAKER_RECEIVED ) == pdTRUE )
                    {
                        prvClientClearState( AIA_STATE_OPENSPEAKER_RECEIVED );
                        pxSpeaker->ullOpenOffset = ullOffset;
                        prvClientOpenSpeaker( pxSpeaker->ullOpenOffset );
                    }

                    pucMsg += sizeof( ullOffset );
                    configPRINTF_DEBUG( ( "DEBUG: Playing seq %u\r\n", ulSeq ) );

                    for( int i = 0; i < ulCount; i++ )
                    {
                        int ret = opus_decode( pxSpeaker->xDecoder,
                                               pucMsg,
                                               AIA_SPEAKER_DECODER_FRAME_SIZE,
                                               sDecodeTemp,
                                               AIA_SPEAKER_MAX_FRAME_SAMPLES,
                                               0 );
                        if( ret != AIA_SPEAKER_RAW_FRAME_SAMPLES )
                        {
                            configPRINTF( ( "opus_decode error %d\r\n", ret ) );
                        }
                        else
                        {
                            int16_t *psData = sDecodeTemp;
                            size_t xBytesSent = 0;
                            for( int i = 0; i < AIA_SPEAKER_RAW_FRAME_SAMPLES; i++ )
                            {
                                *psData = ( *psData * ( int )AIAClient.xSpeaker.ulVolume ) >> 7;
                                psData++;
                            }

                            while( xBytesSent < AIA_SPEAKER_RAW_FRAME_SIZE )
                            {
                                xBytesSent += xStreamBufferSend( pxSpeaker->xDecodeBuffer,
                                                                 sDecodeTemp,
                                                                 sizeof( sDecodeTemp ),
                                                                 pdMS_TO_TICKS( 40 ) );
                            }

                            pucMsg += AIA_SPEAKER_DECODER_FRAME_SIZE;
                        }
                    }
                    pxSpeaker->ullOutputOffset = ullOffset + ulLenAudio;
                }
                else
                {
                    pucMsg += pxBinaryHeader->ulLength;
                }
            }
            else
            {
                uint32_t ulMarker = *( uint32_t * )pucMsg;
                configPRINTF_DEBUG( ( "DEBUG: Marker %u\r\n", ulMarker ) );
                prvClientSendMarker( ulMarker );

                pucMsg += pxBinaryHeader->ulLength;
            }
            xMsgLen -= sizeof( AIABinaryHeader_t ) + pxBinaryHeader->ulLength ;
        }

        if( prvClientGetState( AIA_STATE_SPEAKER_OPENED ) == pdTRUE )
        {
            /* Send UnderrunWarning when available data is less than the threshold and the stream has not reached the end of the speech. */
            if( xBytesRemainedBefore > pxSpeaker->ulSpeakerBufferUnderrunWarning &&
                    xBytesRemained <= pxSpeaker->ulSpeakerBufferUnderrunWarning &&
                    ! ( pxSpeaker->ullCloseOffset > pxSpeaker->ullOpenOffset &&
                            pxSpeaker->ullCloseOffset - pxSpeaker->ullOutputOffset < pxSpeaker->ulSpeakerBufferUnderrunWarning ) )
            {
                xBufferStateChanged.ulSequence = ulSeq;
                xBufferStateChanged.pcBufferStateStr = "UNDERRUN_WARNING";
                prvClientBufferStateChanged( xBufferStateChanged );
            }

            if( ( pxSpeaker->ullCloseOffset > pxSpeaker->ullOpenOffset &&
                    pxSpeaker->ullCloseOffset == pxSpeaker->ullOutputOffset ) ||
                    ( prvClientGetState( AIA_STATE_CLOSESPEAKERNOOFFSET_RECEIVED ) == pdTRUE ) )
            {
                /* In case that a CloseSpeaker directive with no offset is received, close the speaker
                 * at the current output offset.
                 */
                prvClientClearState( AIA_STATE_CLOSESPEAKERNOOFFSET_RECEIVED );
                pxSpeaker->ullCloseOffset = pxSpeaker->ullOutputOffset;

                prvClientCloseSpeaker( pxSpeaker->ullOutputOffset );
            }
        }
    }
}

size_t xClientFillMicrophoneBuffer( void * pvData, size_t xSize, TickType_t xTicksToWait )
{
    return xStreamBufferSend( AIAClient.xMicrophone.xMicBuffer,
                              pvData,
                              xSize,
                              xTicksToWait );
}

size_t xClientFillMicrophoneBufferFromISR( void * pvData, size_t xSize, BaseType_t * pxHigherPriorityTaskWoken )
{
    return xStreamBufferSendFromISR( AIAClient.xMicrophone.xMicBuffer,
                                     pvData,
                                     xSize,
                                     pxHigherPriorityTaskWoken );
}

size_t xClientReadSpeakerBuffer( void * pvData, size_t xSize, TickType_t xTicksToWait )
{
    return xStreamBufferReceive( AIAClient.xSpeaker.xDecodeBuffer,
                                 pvData,
                                 xSize,
                                 xTicksToWait );
}

size_t xClientReadSpeakerBufferFromISR( void * pvData, size_t xSize, BaseType_t * pxHigherPriorityTaskWoken )
{
    return xStreamBufferReceiveFromISR( AIAClient.xSpeaker.xDecodeBuffer,
                                        pvData,
                                        xSize,
                                        pxHigherPriorityTaskWoken );
}

void vClientButtonTapped( void )
{
    AIAClient.pcInitiatorType = "TAP";
    prvClientOpenMicrophone();
    vPlatformTouchButtonDisable();
}

void vClientButtonTappedFromISR( BaseType_t * pxHigherPriorityTaskWoken )
{
    AIAClient.pcInitiatorType = "TAP";
    prvClientOpenMicrophoneFromISR( pxHigherPriorityTaskWoken );
    vPlatformTouchButtonDisable();
}

/* Helper macro if the initialization failed. */
#define CLIENT_INIT_GOTO_FAIL( expr, str )            \
        { if( ( expr ) == true ) {                    \
            configPRINTF( ( str ) );                  \
            goto init_fail; } }

BaseType_t xClientInit( IotMqttConnection_t xMqttConnection )
{
    BaseType_t xReturned;
    int err;

    AIAClient.xInitialized = pdFALSE;

    /* Hardware initializations. */
    xReturned = xPlatformMicrophoneInit();
    CLIENT_INIT_GOTO_FAIL( xReturned != pdPASS, "Failed to initialize platform microphone!\r\n" );

    xReturned = xPlatformSpeakerInit();
    CLIENT_INIT_GOTO_FAIL( xReturned != pdPASS, "Failed to initialize platform speaker!\r\n" );

    xReturned = xPlatformTouchButtonInit();
    CLIENT_INIT_GOTO_FAIL( xReturned != pdPASS, "Failed to initialize platform touch button!\r\n" );

    AIAClient.xMqttConnection = xMqttConnection;
    AIAClient.xDeviceAlerts.xIsSupported = pdFALSE;
    AIAClient.pcASRProfile = "NEAR_FIELD";
    AIAClient.pcInitiatorType = "TAP";
    AIAClient.pcMicrophoneToken = NULL;

    AIAClient.xSpeaker.xIsSupported = pdTRUE;
    AIAClient.xSpeaker.ucChannels = aiaconfigCLIENT_SPEAKER_CHANNELS;
    AIAClient.xSpeaker.ulSampleRate = aiaconfigCLIENT_SPEAKER_SAMPLE_RATE;
    AIAClient.xSpeaker.ulVolume = aiaconfigDEVICE_DEFAULT_VOLUME;
    AIAClient.xSpeaker.ulDecoderBitrate = aiaconfigCLIENT_SPEAKER_DECODER_BITRATE;
    AIAClient.xSpeaker.ulSpeakerBufferSize = aiaconfigCLIENT_SPEAKER_BUFFER_SIZE;
    AIAClient.xSpeaker.ulSpeakerBufferOverrunWarning = aiaconfigCLIENT_SPEAKER_BUFFER_OVERRUN_WARNING;
    AIAClient.xSpeaker.ulSpeakerBufferUnderrunWarning = aiaconfigCLIENT_SPEAKER_BUFFER_UNDERRUN_WARNING;

    AIAClient.xSpeaker.xDecoder = opus_decoder_create( AIAClient.xSpeaker.ulSampleRate,
                                                       AIAClient.xSpeaker.ucChannels,
                                                       &err );
    CLIENT_INIT_GOTO_FAIL( err != OPUS_OK, "Failed to create decoder!\r\n" );

    /* Intialize the context of AES-GCM */
    AIACryptoErrorCode_t cryptoCode = AIA_CRYPTO_Init( &AIAClient.xCrypto, &xKeys );
    CLIENT_INIT_GOTO_FAIL( cryptoCode != eCryptoSuccess, "Failed to initialize AES-GCM!\r\n" );

    AIAClient.xState = xEventGroupCreate();
    CLIENT_INIT_GOTO_FAIL( AIAClient.xState == NULL, "Failed to create xState!\r\n" );

    AIAClient.xMicrophone.xMicBuffer = xStreamBufferCreate( AIA_MICROPHONE_RAW_BUFFER_TOTAL_SIZE, 0 );
    CLIENT_INIT_GOTO_FAIL( AIAClient.xMicrophone.xMicBuffer == NULL, "Failed to create xMicBuffer!\r\n" );

    AIAClient.xSpeaker.xSpeakerBuffer = xMessageBufferCreate( AIAClient.xSpeaker.ulSpeakerBufferSize );
    CLIENT_INIT_GOTO_FAIL( AIAClient.xSpeaker.xSpeakerBuffer == NULL, "Failed to create xSpeakerBuffer!\r\n" );

    AIAClient.xSpeaker.xDecodeBuffer = xStreamBufferCreate( AIA_DECODER_BUFFER_TOTAL_SIZE, 0 );
    CLIENT_INIT_GOTO_FAIL( AIAClient.xSpeaker.xDecodeBuffer == NULL, "Failed to create xDecodeBuffer!\r\n" );

    xReturned = xTaskCreate( prvAIAStreamMicrophoneTask,
                             "AIA_StreamMic",
                             aiaconfigAIA_STREAM_MICROPHONE_TASK_STACK_SIZE,
                             NULL,
                             aiaconfigAIA_STREAM_MICROPHONE_TASK_PRIORITY,
                             &xMicrophoneTaskHandle );
    CLIENT_INIT_GOTO_FAIL( xReturned != pdPASS, "Failed to create AIA_StreamMic task!\r\n" );

    xReturned = xTaskCreate( prvAIASpeakerTask,
                             "AIA_Speaker",
                             aiaconfigAIA_SPEAKER_TASK_STACK_SIZE,
                             NULL,
                             aiaconfigAIA_SPEAKER_TASK_PRIORITY,
                             &xSpeakerTaskHandle );
    CLIENT_INIT_GOTO_FAIL( xReturned != pdPASS, "Failed to create AIA_Speaker task!\r\n" );

    xGenericLock = xSemaphoreCreateMutex();
    CLIENT_INIT_GOTO_FAIL( xGenericLock == NULL, "Failed to create the generic lock!\r\n" );

    AIAClient.xInitialized = pdTRUE;
    configPRINTF( ( "AIA Client initialized!\r\n" ) );

    /* Blink LED to indicate the success of AIA client initialization. */
    xPlatformLEDInit();
    vPlatformLEDBlink( 1000 );

    return pdPASS;

init_fail:
    configPRINTF( ( "Failed to initialize AIA Client!\r\n" ) );
    return pdFAIL;
}

void vClientCleanup( void )
{
    if( AIAClient.xInitialized != pdTRUE )
    {
        return;
    }

    if( xMicrophoneTaskHandle != NULL )
    {
        vTaskDelete( xMicrophoneTaskHandle );
    }
    if( xSpeakerTaskHandle != NULL )
    {
        vTaskDelete( xSpeakerTaskHandle );
    }

    vPlatformLEDOff();

    if( prvClientGetState( AIA_STATE_CONNECTED ) == pdTRUE )
    {
        prvClientDisconnectFromAIA();
    }
}

/* Helper macro if the AIA initialization failed. */
#define CLIENT_AIA_INIT_FAIL( expr )       \
        { if( ( expr ) != pdPASS ) return pdFAIL; }

BaseType_t xClientAIAInit( void )
{
    /* Subscribe to connection/fromservice topic to get response from the server. */
    CLIENT_AIA_INIT_FAIL( prvClientSubscribe( AIA_TOPIC_CONNECTION_SER ) );

    /* Connect to AIA service. */
    CLIENT_AIA_INIT_FAIL( prvClientReconnectToAIA() );

    /* Publish device capabilities. */
    CLIENT_AIA_INIT_FAIL( prvClientPublishCapabilities() );

    /* Synchronize State. */
    CLIENT_AIA_INIT_FAIL( prvClientSynchronizeState() );

    return pdPASS;
}
