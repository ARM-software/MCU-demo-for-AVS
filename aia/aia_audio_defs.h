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

#ifndef _AIA_AUDIO_DEFS_H_
#define _AIA_AUDIO_DEFS_H_

typedef enum AUDIO_CHANNEL_NUM {
    AUDIO_CHANNEL_MONO = 1u,
    AUDIO_CHANNEL_STEREO = 2u
}AudioChannels_t;

#define AUDIO_SAMPLE_RATE_16KHZ         (16000u)

#define AUDIO_SAMPLE_RATE_32KHZ         (32000u)

/*
 * Maximum frame sample size for OPUS codec.
 * From RFC6716 the opus max duration is 120ms.
 * The max frame size = samplerate*channels*120ms/1000ms samples
 */
#define AUDIO_MAX_FRAME_DURATION_MS     (120u)

#endif /* _AIA_AUDIO_DEFS_H_ */
