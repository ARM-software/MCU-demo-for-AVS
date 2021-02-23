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

#ifndef _AIA_UTILS_H_
#define _AIA_UTILS_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "jsmn.h"

/*
 * @brief                       Print a JSON string field which is not null-terminated.
 *
 * @param[in] description       A description string which will be printed before the json string.
 * @param[in] js                The original json string.
 * @param[in] start             The index where the string field begins.
 * @param[in] end               The index where the string field ends.
 */
void vPrintJSONString( const char * description, const uint8_t * js, int start, int end );

/**
 * @brief                       Convert a JSON value field into a number.
 *
 * @param[in] js                The original json string.
 * @param[in] start             The index where the value field begins.
 * @param[in] end               The index where the value field ends.
 *
 * @return                      The converted number of the value field.
 */
uint64_t ullConvertJSONLong( const uint8_t * js, int start, int end );

/**
 * @brief                       Check if an array of characters is equal to a string without the null terminator.
 *
 * @param[in] pcArray           The character array.
 * @param[in] xArrayLength      The length of the array.
 * @param[in] pcString          The compared string.
 *
 * @return                      `pdTRUE` when equal. `pdFALSE` otherwise.
 */
BaseType_t xIsStringEqual( const uint8_t * pcArray, const size_t xArrayLength, const char * pcString );

/**
 * @brief                       Check if a non null-terminated string is equal to a topic name.
 *
 * @param[in] pcUserTopic       The non null-terminated string.
 * @param[in] xUserTopicLength  The length of the non null-terminated string.
 * @param[in] pcTargetTopic     The compared topic name.
 *
 * @return                      `pdTRUE` when equal. `pdFALSE` otherwise.
 */
BaseType_t xIsTopic( const char * pcUserTopic, const size_t xUserTopicLength, const char * pcTargetTopic );

/**
 * @brief                           Check if a non null-terminated string is equal to a directive name.
 *
 * @param[in] pcUserDirective       The non null-terminated string.
 * @param[in] xUserDirectiveLength  The length of the non null-terminated string.
 * @param[in] pcTargetDirective     The compared directive name.
 *
 * @return                          `pdTRUE` when equal. `pdFALSE` otherwise.
 */
BaseType_t xIsDirective( const uint8_t * pcUserDirective, const size_t xUserDirectiveLength, const char * pcTargetDirective );

/**
 * @brief                           Parse a json message into tokens using jsmn library.
 *
 * @param[in] pcMessage             The json message to be parsed.
 * @param[in] xMessageLength        The length of the json message.
 * @param[in] pxJSMNTokens          Pointer to an array of jsmn tokens to store the parsed results.
 * @param[in] ulMaxTokenNumber      The maximum number of tokens that the array can hold.
 *
 * @return                          The total number of parsed tokens on success.
 *                                  Otherwise a enum jsmnerr type to indicate the error.
 */
int32_t lParseJSMN( const char * pcMessage, size_t xMessageLength, jsmntok_t * pxJSMNTokens, uint32_t ulMaxTokenNumber );

#endif /* _AIA_UTILS_H_ */
