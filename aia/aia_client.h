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

#ifndef _AIA_CLIENT_H_
#define _AIA_CLIENT_H_

#include "FreeRTOS.h"
#include "task.h"
#include "iot_mqtt.h"

#define AIA_MSG_PARAMS_SIZE_SEQ         ( 4 )
#define AIA_MSG_PARAMS_SIZE_IV          ( 12 )
#define AIA_MSG_PARAMS_SIZE_MAC         ( 16 )

/* A dummy representation of AIA messages only for address calculation. */
typedef struct __attribute__ ((__packed__)){
    uint8_t sequence[ AIA_MSG_PARAMS_SIZE_SEQ ];
    uint8_t iv[ AIA_MSG_PARAMS_SIZE_IV ];
    uint8_t mac[ AIA_MSG_PARAMS_SIZE_MAC ];
    uint8_t ciphertext[ 0 ];
} AIAMessage_t;

/* The handle of the demo task if any so that it could be signaled by the AIA client. */
extern TaskHandle_t xDemoTaskHandle;

/**
 * @brief Initialize the AIA client.
 *
 * This function initializes the AIA client with an established mqtt connection.
 *
 * @param[in] xMqttConnection                   An Established mqtt connection.
 *
 * @return                                      pdPASS on success and pdFAIL on failure.
 *
 */
BaseType_t xClientInit( IotMqttConnection_t xMqttConnection );

/**
 * @brief AIA client cleanup.
 *
 */
void vClientCleanup( void );

/**
 * @brief Connect to AIA service and reach IDLE state.
 *
 * This function connects the AIA client to AIA service and get the client ready for voice interaction.
 *
 * @return                                      pdPASS on success and pdFAIL on failure.
 *
 */
BaseType_t xClientAIAInit( void );

/**
 * @brief Fill the client microphone buffer.
 *
 * This function sends data to the client microphone buffer. The bytes are copied into the buffer. A timeout can
 * be specified for how long the function can wait.
 * This function CANNOT be called from an ISR context. Use the interrupt-safe version instead.
 *
 * @param[in] pvData                            A pointer to the data to be sent.
 * @param[in] xSize                             The total bytes of data to be sent.
 * @param[in] xTicksToWait                      Timeout value for how long this function can wait.
 *
 * @return                                      Number of bytes actually being sent.
 *
 */
size_t xClientFillMicrophoneBuffer( void * pvData, size_t xSize, TickType_t xTicksToWait );

/**
 * @brief Fill the client microphone buffer from interrupt.
 *
 * The interrupt-safe version of xClientSendToMicrophone(). This is usually called from the DMA interrupt, for
 * instance, when some data is received from the platform microphone.
 *
 * @param[in] pvData                            A pointer to the data to be sent.
 * @param[in] xSize                             The total bytes of data to be sent.
 * @param[out] pxHigherPriorityTaskWoken        The function sets *pxHigherPriorityTaskWoken to pdTRUE if a context
 *                                              switch should be performed before the interrupt is exited, that is
 *                                              when a higher priority task is woken up by this function.
 *                                              *pxHigherPriorityTaskWoken should be set to pdFALSE before it is
 *                                              passed into the function.
 *
 * @return                                      Number of bytes actually being sent.
 *
 */
size_t xClientFillMicrophoneBufferFromISR( void * pvData, size_t xSize, BaseType_t * pxHigherPriorityTaskWoken );

/**
 * @brief Receive data from the client speaker buffer.
 *
 * This function receives data from the client speaker buffer. A timeout can be specified for how long the function
 * can wait.
 * This function CANNOT be called from an ISR context. Use the interrupt-safe version instead.
 *
 * @param[in] pvData                            A pointer to the buffer that receives data.
 * @param[in] xSize                             The total bytes of data to be received.
 * @param[in] xTicksToWait                      Timeout value for how long this function can wait.
 *
 * @return                                      Number of bytes actually being received.
 *
 */
size_t xClientReadSpeakerBuffer( void * pvData, size_t xSize, TickType_t xTicksToWait );

/**
 * @brief Receive data from the client speaker buffer from interrupt.
 *
 * The interrupt-safe version of xClientReceiveFromSpeaker(). This is usually called from the DMA interrupt, for
 * instance, when it needs to feed new data to the audio hardware for playback.
 *
 * @param[in] pvData                            A pointer to the buffer that receives data.
 * @param[in] xSize                             The total bytes of data to be received.
 * @param[out] pxHigherPriorityTaskWoken        The function sets *pxHigherPriorityTaskWoken to pdTRUE if a context
 *                                              switch should be performed before the interrupt is exited, that is
 *                                              when a higher priority task is woken up by this function.
 *                                              *pxHigherPriorityTaskWoken should be set to pdFALSE before it is
 *                                              passed into the function.
 *
 * @return                                      Number of bytes actually being received.
 *
 */
size_t xClientReadSpeakerBufferFromISR( void * pvData, size_t xSize, BaseType_t * pxHigherPriorityTaskWoken );

/**
 * @brief The function that should be called when the touch button is tapped.
 *
 * This function should be called when a tap action is detected on the platform touch button.
 * This function CANNOT be called from an ISR context. Use the interrupt-safe version instead.
 *
 */
void vClientButtonTapped( void );

/**
 * @brief The function that should be called when the touch button is tapped from interrupt.
 *
 * The interrupt-safe version of xClientButtonTapped().
 *
 */
void vClientButtonTappedFromISR( BaseType_t * pxHigherPriorityTaskWoken );

#endif /* _AIA_CLIENT_H_ */
