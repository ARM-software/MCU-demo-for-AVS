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

#ifndef _AIA_PLATFORM_H_
#define _AIA_PLATFORM_H_

#include <stdint.h>
#include "FreeRTOS.h"

/**
 * @brief The function that initializes platform LED for demo status indication.
 *
 * @return `pdPASS` if the initialization completes successfully; `pdFAIL` otherwise.
 */
BaseType_t xPlatformLEDInit( void );

/**
 * @brief The function that initializes platform microphone.
 *
 * @return `pdPASS` if the initialization completes successfully; `pdFAIL` otherwise.
 */
BaseType_t xPlatformMicrophoneInit( void );

/**
 * @brief The function that initializes platform speaker.
 *
 * @return `pdPASS` if the initialization completes successfully; `pdFAIL` otherwise.
 */
BaseType_t xPlatformSpeakerInit( void );

/**
 * @brief The function that initializes platform touch button.
 *
 * @return `pdPASS` if the initialization completes successfully; `pdFAIL` otherwise.
 */
BaseType_t xPlatformTouchButtonInit( void );

/**
 * @brief The function that turns on LED.
 */
void vPlatformLEDOn( void );

/**
 * @brief The function that turns off LED.
 */
void vPlatformLEDOff( void );

/**
 * @brief The function that blinks LED.
 *
 * @param[in] interval_ms The interval that LED should blink in milliseconds.
 */
void vPlatformLEDBlink( uint32_t interval_ms );

/**
 * @brief The function that opens microphone for voice capture.
 */
void vPlatformMicrophoneOpen( void );

/**
 * @brief The function that closes microphone to stop voice capture.
 */
void vPlatformMicrophoneClose( void );

/**
 * @brief The function that opens speaker for audio playback.
 */
void vPlatformSpeakerOpen( void );

/**
 * @brief The function that closes speaker to stop audio playback.
 */
void vPlatformSpeakerClose( void );

/**
 * @brief The function that enables touch button for tap detection.
 */
void vPlatformTouchButtonEnable( void );

/**
 * @brief The function that disables touch button.
 */
void vPlatformTouchButtonDisable( void );

#endif /* _AIA_PLATFORM_H_ */
