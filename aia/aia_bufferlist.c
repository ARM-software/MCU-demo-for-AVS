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

#include "aia_bufferlist.h"

enum {
    eBufferListInsert,
    eBufferListReplace,
};

static int prvFindNodeLocation( AIABufferListNode_t ** ppxListNode, uint32_t ulSequence )
{
    AIABufferListNode_t * pxListHead;

    pxListHead = *ppxListNode;

    /* As the list nodes are inserted in ascending order, search from the rear to front. */
    *ppxListNode = pxListHead->pxPrevious;

    while ( *ppxListNode != pxListHead )
    {
        const void * pvData = ( *ppxListNode )->pvData;

        /* The first four bytes pointed by pvData should be the sequence number. */
        if( *( uint32_t * )pvData == ulSequence )
        {
            return eBufferListReplace;
        }
        else if( *( uint32_t * )pvData > ulSequence )
        {
            *ppxListNode = ( *ppxListNode )->pxPrevious;
        }
        else
        {
            break;
        }
    };

    return eBufferListInsert;
}

BaseType_t xAIABufferListInitialize( AIABufferList_t * pxBufferList )
{
    AIABufferListNode_t * pxListHead;

    pxListHead = ( AIABufferListNode_t * )pvPortMalloc( sizeof( AIABufferListNode_t ) );
    if( pxListHead == NULL )
    {
        return pdFAIL;
    }

    /* Point the head node to itself. */
    pxListHead->pvData = NULL;
    pxListHead->xDataSize = 0;
    pxListHead->pxNext = pxListHead;
    pxListHead->pxPrevious = pxListHead;

    pxBufferList->pxListHead = pxListHead;

    return pdPASS;
}

void vAIABufferListDestroy( AIABufferList_t * pxBufferList )
{
    AIABufferListNode_t * pxListNode;

    configASSERT( pxBufferList->pxListHead != NULL );

    pxListNode = pxBufferList->pxListHead;

    do
    {
        pxListNode = pxListNode->pxNext;
        vPortFree( pxListNode->pxPrevious );
    } while ( pxListNode != pxBufferList->pxListHead );

    pxBufferList->pxListHead = NULL;
}

BaseType_t xAIABufferListInsert( AIABufferList_t * pxBufferList, const void * pvData, size_t xDataSize )
{
    AIABufferListNode_t * pxListNode;
    AIABufferListNode_t * pxNewNode;
    int lActionToTake;

    configASSERT( pxBufferList->pxListHead != NULL );

    pxListNode = pxBufferList->pxListHead;

    /* Find where the node should be inserted by its sequence number. */
    lActionToTake = prvFindNodeLocation( &pxListNode, *( uint32_t *)pvData );
    configASSERT( ( lActionToTake == eBufferListReplace) || ( lActionToTake == eBufferListInsert) );

    if( lActionToTake == eBufferListReplace )
    {
        /* A node of the same sequence number is found, just replace the content. */
        pxListNode->pvData = pvData;
        pxListNode->xDataSize = xDataSize;
    }
    else if( lActionToTake == eBufferListInsert )
    {
        /* Allocate a new node and insert it after the found location. */
        pxNewNode = ( AIABufferListNode_t * )pvPortMalloc( sizeof( AIABufferListNode_t ) );
        if( pxNewNode == NULL )
        {
            return pdFAIL;
        }

        pxNewNode->pvData = pvData;
        pxNewNode->xDataSize = xDataSize;

        pxListNode->pxNext->pxPrevious = pxNewNode;
        pxNewNode->pxNext = pxListNode->pxNext;
        pxListNode->pxNext = pxNewNode;
        pxNewNode->pxPrevious = pxListNode;
    }

    return pdPASS;
}

uint32_t ulAIABufferListFirstSequence( AIABufferList_t * pxBufferList )
{
    const void * pvData;
    uint32_t ulSequence;

    configASSERT( pxBufferList->pxListHead != NULL );

    pvData = pxBufferList->pxListHead->pxNext->pvData;

    if( pvData == NULL )
    {
        /* It is in fact inappropriate to return the next sequence number in the list as 0
         * if the list is empty, because by right the No.0 message could also be stored in
         * the list. If we want to distinguish between these two cases, we would need to
         * enlarge the return type so a minus number could be returned for the empty case.
         * We might change this if future usage of the buffer list demands it.
         */
        ulSequence = 0;
    }
    else
    {
        ulSequence = *( uint32_t * )pvData;
    }

    return ulSequence;
}

size_t xAIABufferListPopFirstMessage( AIABufferList_t * pxBufferList, const void ** ppvData )
{
    AIABufferListNode_t * pxListNode;
    size_t xDataSize;

    configASSERT( pxBufferList->pxListHead != NULL );

    pxListNode = pxBufferList->pxListHead->pxNext;
    *ppvData = pxListNode->pvData;
    xDataSize = pxListNode->xDataSize;

    /* Remove this node. */
    if( pxListNode != pxBufferList->pxListHead )
    {
        pxListNode->pxPrevious->pxNext = pxListNode->pxNext;
        pxListNode->pxNext->pxPrevious = pxListNode->pxPrevious;
        vPortFree( pxListNode );
    }

    return xDataSize;
}
