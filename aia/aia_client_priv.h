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

#ifndef _AIA_CLIENT_PRIV_H_
#define _AIA_CLIENT_PRIV_H_

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "stream_buffer.h"
#include "message_buffer.h"

/* Credentials includes. */
#include "aws_clientcredential.h"

#include "aia_client_config.h"
#include "aia_client.h"
#include "aia_crypto.h"

#include "opus.h"

/* Topic macros */
#define AIA_TOPIC                       "ais"
#define AIA_TOPIC_HEAD                  aiaconfigTOPIC_ROOT"/"AIA_TOPIC"/"aiaconfigAPI_VERSION"/"clientcredentialIOT_THING_NAME

#define AIA_TOPIC_CONNECTION_CLI        AIA_TOPIC_HEAD"/connection/fromclient"
#define AIA_TOPIC_CONNECTION_SER        AIA_TOPIC_HEAD"/connection/fromservice"
#define AIA_TOPIC_CAPABILITIES_PUB      AIA_TOPIC_HEAD"/capabilities/publish"
#define AIA_TOPIC_CAPABILITIES_ACK      AIA_TOPIC_HEAD"/capabilities/acknowledge"
#define AIA_TOPIC_DIRECTIVE             AIA_TOPIC_HEAD"/directive"
#define AIA_TOPIC_EVENT                 AIA_TOPIC_HEAD"/event"
#define AIA_TOPIC_MICROPHONE            AIA_TOPIC_HEAD"/microphone"
#define AIA_TOPIC_SPEAKER               AIA_TOPIC_HEAD"/speaker"

/* Message macros */
#define AIA_MSG_CONNECT                 "{\"header\" : {\"name\": \"Connect\",\"messageId\" : \"0\"}, " \
                                        "\"payload\" : {\"awsAccountId\" : \""aiaconfigAWS_ACCOUNT_ID"\", \"clientId\" : \""clientcredentialIOT_THING_NAME"\" }}"
#define AIA_MSG_DISCONNECT              "{\"header\" : {\"name\": \"Disconnect\",\"messageId\" : \"disconnecting_message\"}, " \
                                        "\"payload\" : {\"code\" : \"GOING_OFFLINE\", \"description\" : \""clientcredentialIOT_THING_NAME" disconnecting\" }}"

/* The token number of the name parameter in a jsmntok_t array.
 * Fixed locations are used for JSMN tokens. This is not portable,
 * but we would otherwise need to search for the required field in
 * the array, which is inefficient. Might be changed in the future.
 */
#define AIA_MSGTOKENPOS_CONNECTION_NAME                 ( 4 )
#define AIA_MSGTOKENPOS_CONNECTION_CODE                 ( 12 )
#define AIA_MSGTOKENPOS_DISCONNECTION_CODE              ( 10 )
#define AIA_MSGTOKENPOS_DISCONNECTION_DESCRIPTION       ( 12 )

#define AIA_MSGTOKENPOS_CAPABILITIES_CODE               ( 12 )
#define AIA_MSGTOKENPOS_CAPABILITIES_DESCRIPTION        ( 14 )

#define AIA_MSGTOKENPOS_DIRECTIVE_ARRAY                 ( 2 )
/* Relative token index inside a directive object. */
#define AIA_MSGTOKENPOS_DIRECTIVE_NAME                  ( 4 )
#define AIA_MSGTOKENPOS_SETATTENTIONSTATE_STATE         ( 10 )
#define AIA_MSGTOKENPOS_SETATTENTIONSTATE_OFFSET        ( 12 )
#define AIA_MSGTOKENPOS_SETVOLUME_VOLUME                ( 10 )
#define AIA_MSGTOKENPOS_SETVOLUME_OFFSET                ( 12 )
#define AIA_MSGTOKENPOS_OPENSPEAKER_OFFSET              ( 10 )
#define AIA_MSGTOKENPOS_CLOSESPEAKER_OFFSET             ( 10 )
#define AIA_MSGTOKENPOS_INITIATOR                       ( 12 )

/* Size in token number of different directives.
 * Note that these value does not count for optional fields.
 * They must be taken care of in the code.
 */
#define AIA_MSGTOKENSIZE_SETATTENTIONSTATE              ( 11 )
#define AIA_MSGTOKENSIZE_OPENSPEAKER                    ( 11 )
#define AIA_MSGTOKENSIZE_CLOSESPEAKER                   ( 7 )
#define AIA_MSGTOKENSIZE_CLOSEMICROPHONE                ( 7 )
#define AIA_MSGTOKENSIZE_OPENMICROPHONE                 ( 11 )
#define AIA_MSGTOKENSIZE_SETVOLUME                      ( 11 )

/* Client state */
enum {
    sConnected = 0,
    sConnectionDenied,
    sCapabilitiesAccepted,
    sCapabilitiesRejected,
    sMicrophoneOpened,
    sSpeakerOpened,
    sOpenSpeakerReceived,
    sCloseSpeakerNoOffsetReceived,
    sAlexaIdle,
    sAlexaThinking,
    sAlexaSpeaking,
    sAlexaAlerting,
    sMax = 32
};

#define AIA_STATE_CONNECTED                             ( 1 << sConnected )
#define AIA_STATE_CONNECTION_DENIED                     ( 1 << sConnectionDenied )
#define AIA_STATE_CAPABILITIES_ACCEPTED                 ( 1 << sCapabilitiesAccepted )
#define AIA_STATE_CAPABILITIES_REJECTED                 ( 1 << sCapabilitiesRejected )
#define AIA_STATE_MICROPHONE_OPENED                     ( 1 << sMicrophoneOpened )
#define AIA_STATE_SPEAKER_OPENED                        ( 1 << sSpeakerOpened )
#define AIA_STATE_OPENSPEAKER_RECEIVED                  ( 1 << sOpenSpeakerReceived )
#define AIA_STATE_CLOSESPEAKERNOOFFSET_RECEIVED         ( 1 << sCloseSpeakerNoOffsetReceived )
#define AIA_STATE_ALEXA_IDLE                            ( 1 << sAlexaIdle )
#define AIA_STATE_ALEXA_THINKING                        ( 1 << sAlexaThinking )
#define AIA_STATE_ALEXA_SPEAKING                        ( 1 << sAlexaSpeaking )
#define AIA_STATE_ALEXA_ALERTING                        ( 1 << sAlexaAlerting )
#define AIA_STATE_ALEXA_MASK                            ( AIA_STATE_ALEXA_IDLE | AIA_STATE_ALEXA_THINKING | AIA_STATE_ALEXA_SPEAKING | AIA_STATE_ALEXA_ALERTING )

#define AIA_SPEAKER_DECODER_FRAME_SIZE                  ( aiaconfigCLIENT_SPEAKER_DECODER_BITRATE * aiaconfigCLIENT_SPEAKER_FRAME_DURATION_MS / 1000 / 8 )
#define AIA_SPEAKER_RAW_BYTES_PER_SAMPLE                ( aiaconfigCLIENT_SPEAKER_CHANNELS * aiaconfigCLIENT_SPEAKER_SAMPLE_RESOLUTION / 8 )
#define AIA_SPEAKER_RAW_FRAME_SAMPLES                   ( aiaconfigCLIENT_SPEAKER_SAMPLE_RATE * aiaconfigCLIENT_SPEAKER_FRAME_DURATION_MS / 1000 )
#define AIA_SPEAKER_RAW_FRAME_SIZE                      ( AIA_SPEAKER_RAW_FRAME_SAMPLES * AIA_SPEAKER_RAW_BYTES_PER_SAMPLE)
#define AIA_SPEAKER_MAX_FRAME_SAMPLES                   ( aiaconfigCLIENT_SPEAKER_SAMPLE_RATE * AUDIO_MAX_FRAME_DURATION_MS / 1000 )
#define AIA_SPEAKER_MAX_FRAME_SIZE                      ( AIA_SPEAKER_MAX_FRAME_SAMPLES * AIA_SPEAKER_RAW_BYTES_PER_SAMPLE)
#define AIA_SPEAKER_COMPRESSION_RATE                    ( AIA_SPEAKER_RAW_FRAME_SIZE / AIA_SPEAKER_DECODER_FRAME_SIZE )
#define AIA_DECODER_BUFFER_TOTAL_SIZE                   ( AIA_SPEAKER_RAW_FRAME_SIZE * aiaconfigCLIENT_DECODER_BUFFER_FRAMES )

#define AIA_MICROPHONE_RAW_BYTES_PER_SAMPLE             ( aiaconfigCLIENT_MICROPHONE_RAW_SAMPLE_RESOLUTION / 8 )
#define AIA_MICROPHONE_RAW_FRAME_SAMPLES                ( aiaconfigCLIENT_MICROPHONE_RAW_CHANNELS * aiaconfigCLIENT_MICROPHONE_RAW_SAMPLE_RATE * aiaconfigCLIENT_MICROPHONE_RAW_FRAME_DURATION_MS / 1000 )
#define AIA_MICROPHONE_RAW_FRAME_SIZE                   ( AIA_MICROPHONE_RAW_FRAME_SAMPLES * AIA_MICROPHONE_RAW_BYTES_PER_SAMPLE )
#define AIA_MICROPHONE_RAW_BUFFER_TOTAL_SIZE            ( AIA_MICROPHONE_RAW_FRAME_SIZE * aiaconfigCLIENT_MICROPHONE_RAW_BUFFER_FRAMES )

typedef enum {
    aiaEventSecretRotated,
    aiaEventButtonCommandIssued,
    aiaEventSpeakerOpened,
    aiaEventSpeakerClosed,
    aiaEventSpeakerMarkerEncountered,
    aiaEventMicrophoneOpened,
    aiaEventMicrophoneClosed,
    aiaEventOpenMicrophoneTimedOut,
    aiaEventBufferStateChanged,
    aiaEventVolumeChanged,
    aiaEventSynchronizeClock,
    aiaEventSetAlertSucceeded,
    aiaEventSetAlertFailed,
    aiaEventDeleteAlertSucceeded,
    aiaEventDeleteAlertFailed,
    aiaEventAlertVolumeChanged,
    aiaEventSynchronizeState,
    aiaEventExceptionenCountered,
    aiaEventStopPlaying,
} AIAEvent_t;

typedef struct {
    struct {
        uint32_t ulLen;
        uint8_t ucBuffer[ aiaconfigAIA_MESSAGE_MAX_SIZE ];
    } xMessage [ aiaconfigAIA_SPEAKER_RESEQUENCING ];
    uint8_t ucStartIndex;
} AIAClient_Resequence_t;

typedef struct __attribute__((__packed__)){
    uint32_t ulLength;
    uint8_t  ucType;
    uint8_t  ucCount;
    uint16_t usReserved;
} AIABinaryHeader_t;

typedef struct __attribute__((__packed__)) {
    AIABinaryHeader_t xHeader;
    uint64_t ullOffset;
    uint8_t ucAudio[aiaconfigAIA_AUDIO_DATA_SIZE];
} AIABinaryAudioStream_t;

typedef struct {
    uint32_t ulSequence;
    char * pcBufferStateStr;
} AIABufferStateChanged_t;

typedef struct {
    char * pcWakeWordString;
    uint64_t ullWakeWordBegin;
    uint64_t ullWakeWordEnd;
} AIAClient_Wakeword_t;

typedef struct {
    BaseType_t xIsSupported;
    uint8_t ucChannels;
    uint32_t ulSampleRate;
    uint32_t ulVolume;
    uint64_t ullOpenOffset;
    uint64_t ullCloseOffset;
    uint64_t ullOutputOffset;
    OpusDecoder * xDecoder;
    uint32_t ulDecoderBitrate;
    /* The speaker buffer is currently implemented using FreeRTOS message buffers.
     * However, AIA has some special requirements for buffer management which message
     * buffers cannot satisfy, e.g. message replacement and re-ordering. We have to
     * introduce another buffer for resequencing in aia_client as one of the drawbacks
     * of the current method. A better solution is to implement customized buffer with
     * other data structures e.g. linked list.
     */
    MessageBufferHandle_t xSpeakerBuffer;
    uint32_t ulSpeakerBufferSize;
    uint32_t ulSpeakerBufferOverrunWarning;
    uint32_t ulSpeakerBufferUnderrunWarning;
    StreamBufferHandle_t xDecodeBuffer;
} AIAClient_Speaker_t;

typedef struct {
    BaseType_t xIsSupported;
    char ** ppcAlerts;
    uint32_t ulAlertsNum;
} AIAClient_Alerts_t;

typedef struct {
    uint32_t ulVolume;
    uint64_t ullOffset;
} AIAClient_SetVolume_t;

typedef struct {
    StreamBufferHandle_t xMicBuffer;
    uint32_t ulMicrophoneSequence;
    uint64_t ullMicrophoneOffset;
} AIAClient_Microphone_t;

typedef struct {
    BaseType_t xInitialized;
    IotMqttConnection_t xMqttConnection;
    EventGroupHandle_t xState;
    AIAClient_Speaker_t xSpeaker;
    AIAClient_Alerts_t xDeviceAlerts;
    char * pcASRProfile;
    char * pcInitiatorType;
    char * pcMicrophoneToken;
    AIAClient_Wakeword_t xWakeword;
    AIAClient_Microphone_t xMicrophone;
    AIACrypto_t xCrypto;
} AIAClient_t;

#ifdef DEBUG
#define printJSONString_DEBUG( X )          prvPrintJSONString X
#else
#define printJSONString_DEBUG( X )
#endif

#define AIA_EVENT_MESSAGE_MAX_SIZE  ( 1024 )

#endif /* _AIA_CLIENT_PRIV_H_ */
