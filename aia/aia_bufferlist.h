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

#ifndef _AIA_LIST_H_
#define _AIA_LIST_H_

#include <stdint.h>
#include "FreeRTOS.h"

struct AIABufferListNode;
struct AIABufferList;

struct AIABufferListNode {
    const void * pvData;
    size_t xDataSize;
    struct AIABufferListNode * pxPrevious;
    struct AIABufferListNode * pxNext;
};

typedef struct AIABufferListNode AIABufferListNode_t;

struct AIABufferList {
    struct AIABufferListNode * pxListHead;
};

typedef struct AIABufferList AIABufferList_t;

/**
 * @brief                   Initialize a buffer list.
 *
 * @param[in] pxBufferList  Pointer to the list to be initialized.
 *
 * @return                  `pdPASS` on success; `pdFAIL` otherwise.
 */
BaseType_t xAIABufferListInitialize( AIABufferList_t * pxBufferList );

/**
 * @brief                   Destroy a buffer list.
 *
 * @param[in] pxBufferList  Pointer to the list to be destroyed.
 */
void vAIABufferListDestroy( AIABufferList_t * pxBufferList );

/**
 * @brief                   Create a new node for a new message and insert into the buffer list by its sequence number.
 *                          If an old message with the same sequence number is found, replace it with the new one.
 *
 * @param[in] pxBufferList  Pointer to the list.
 * @param[in] pvData        Pointer to the message to be inserted. The message should start with a four-byte sequence number.
 * @param[in] xDataSize     The size in bytes of the message.
 *
 * @return                  `pdPASS` on success; `pdFAIL` otherwise.
 */
BaseType_t xAIABufferListInsert( AIABufferList_t * pxBufferList, const void * pvData, size_t xDataSize );

/**
 * @brief                   Return the sequence number of the first message in the buffer list.
 *
 * @param[in] pxBufferList  Pointer to the list.
 *
 * @return                  The sequence number of the first message in the buffer list.
 */
uint32_t ulAIABufferListFirstSequence( AIABufferList_t * pxBufferList );

/**
 * @brief                   Pop out the first message in the buffer list. The node is destroyed on return.
 *
 * @param[in] pxBufferList  Pointer to the list.
 * @param[out] ppvData      Pointer to the address of the first message.
 *
 * @return                  The size of the first message.
 */
size_t xAIABufferListPopFirstMessage( AIABufferList_t * pxBufferList, const void ** ppvData );

#endif /* _AIA_LIST_H_ */
