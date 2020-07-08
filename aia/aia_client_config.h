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

#ifndef _AIA_CLIENT_CONFIG_H_
#define _AIA_CLIENT_CONFIG_H_

#include "aia_audio_defs.h"

#define aiaconfigAWS_ACCOUNT_ID                             ""

#define aiaconfigTOPIC_ROOT                                 ""

#define aiaconfigCLIENT_PUBLIC_KEY                          ""

#define aiaconfigCLIENT_PRIVATE_KEY                         ""

#define aiaconfigPEER_PUBLIC_KEY                            ""

#define aiaconfigAPI_VERSION                                "v1"

#define aiaconfigCLIENT_MICROPHONE_RAW_SAMPLE_RATE          AUDIO_SAMPLE_RATE_16KHZ

#define aiaconfigCLIENT_MICROPHONE_RAW_CHANNELS             AUDIO_CHANNEL_MONO

#define aiaconfigCLIENT_MICROPHONE_RAW_FRAME_DURATION_MS    ( 20UL )

/* PDM mic supports 16/24/32bits raw data, sample resolution should be 16 or 32bits */
#define aiaconfigCLIENT_MICROPHONE_RAW_SAMPLE_RESOLUTION    ( 16UL )

#define aiaconfigCLIENT_MICROPHONE_RAW_BUFFER_FRAMES        ( 10UL )

#define aiaconfigCLIENT_SPEAKER_BUFFER_SIZE                 ( 32000UL )

#define aiaconfigCLIENT_SPEAKER_BUFFER_OVERRUN_WARNING      ( 22000UL )

#define aiaconfigCLIENT_SPEAKER_BUFFER_UNDERRUN_WARNING     ( 10000UL )

#define aiaconfigCLIENT_DECODER_BUFFER_FRAMES               ( 1UL )

#define aiaconfigCLIENT_SPEAKER_CHANNELS                    AUDIO_CHANNEL_MONO

#define aiaconfigCLIENT_SPEAKER_SAMPLE_RATE                 AUDIO_SAMPLE_RATE_16KHZ

#define aiaconfigCLIENT_SPEAKER_SAMPLE_RESOLUTION           ( 16UL )

#define aiaconfigCLIENT_SPEAKER_FRAME_DURATION_MS           ( 20UL )

#define aiaconfigCLIENT_SPEAKER_DECODER_BITRATE             ( 64000UL )

#define aiaconfigAIA_MESSAGE_MAX_SIZE                       ( 5400UL )

#define aiaconfigAIA_AUDIO_DATA_SIZE                        ( 4800UL )

#define aiaconfigAIA_DEFAULT_TIMEOUT                        pdMS_TO_TICKS( 5000 )

#define aiaconfigAIA_RECONNECT_RETRY                        ( 5UL )

#define aiaconfigAIA_RECONNECT_INTERVAL                     pdMS_TO_TICKS( 200 )

/* The range of volume is between 0 and 100. */
#define aiaconfigDEVICE_DEFAULT_VOLUME                      ( 100UL )

/* Maximum jsmn token numbers. */
#define aiaconfigJSMN_MAX_TOKENS                            ( 64UL )

#define aiaconfigAIA_STREAM_MICROPHONE_TASK_STACK_SIZE      ( configMINIMAL_STACK_SIZE * 4 )
#define aiaconfigAIA_STREAM_MICROPHONE_TASK_PRIORITY        ( tskIDLE_PRIORITY + 3 )

#define aiaconfigAIA_SPEAKER_TASK_STACK_SIZE                ( configMINIMAL_STACK_SIZE * 18 )
#define aiaconfigAIA_SPEAKER_TASK_PRIORITY                  ( configMAX_PRIORITIES - 2 )

/* The number of out-of-order messages received on /speaker that we handle. */
#define aiaconfigAIA_SPEAKER_RESEQUENCING                   ( 4UL )

#endif
