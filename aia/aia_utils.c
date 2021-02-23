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

#include "aia_utils.h"

void vPrintJSONString( const char * description, const uint8_t * js, int start, int end )
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

uint64_t ullConvertJSONLong( const uint8_t * js, int start, int end )
{
    uint64_t num = 0;
    for( ; start < end; start++ )
    {
        num = num * 10 + *( js + start ) - '0';
    }
    return num;
}

BaseType_t xIsStringEqual( const uint8_t * pcArray, const size_t xArrayLength, const char * pcString )
{
    BaseType_t xReturned;

    if( xArrayLength == strlen( pcString ) &&
            memcmp( pcArray, pcString, xArrayLength ) == 0 )
    {
        xReturned = pdTRUE;
    }
    else
    {
        xReturned = pdFALSE;
    }

    return xReturned;
}

BaseType_t xIsTopic( const char * pcUserTopic, const size_t xUserTopicLength, const char * pcTargetTopic )
{
    return xIsStringEqual( ( const uint8_t * )pcUserTopic, xUserTopicLength, pcTargetTopic );
}

BaseType_t xIsDirective( const uint8_t * pcUserDirective, const size_t xUserDirectiveLength, const char * pcTargetDirective )
{
    return xIsStringEqual( pcUserDirective, xUserDirectiveLength, pcTargetDirective );
}

int32_t lParseJSMN( const char * pcMessage, size_t xMessageLength, jsmntok_t * pxJSMNTokens, uint32_t ulMaxTokenNumber )
{
    int32_t lNbTokens;
    jsmn_parser xJSMNParser;

    jsmn_init( &xJSMNParser );
    lNbTokens = jsmn_parse( &xJSMNParser, pcMessage, xMessageLength, pxJSMNTokens, ulMaxTokenNumber );
    if( lNbTokens < 0 )
    {
        configPRINTF( ( "Failed to parse received message! Error: %d\r\n", lNbTokens ) );
    }

    return lNbTokens;
}
