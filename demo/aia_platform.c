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

#include "aia_platform.h"
#include "aia_client.h"
#include "task.h"
#include "cycfg.h"
#include "cycfg_capsense.h"

#define CAPSENSE_INTERRUPT_PRIORITY     ( 7u )
#define CAPSENSE_SCAN_TASK_PRIORITY     ( configMAX_PRIORITIES - 1 )
#define CAPSENSE_SCAN_TASK_STACK_SIZE   ( configMINIMAL_STACK_SIZE )
#define CAPSENSE_SCAN_INTERVAL_MS       ( 10u )

/* Samples transferred by DMA each time. */
#define DMA_RECORD_BUFFER_SAMPLES       ( 320 )

/* Samples stored in buffer to be sent to I2S by DMA each time. */
/* As the data stored in buffer is mono-channel audio, DMA is configured as
 * "2D" mode. Two samples are transferred in each X loop which are duplicate. As
 * the maximum value of Y loop count is 256, 160 is chosen here for each transfer.
 */
#define DMA_PLAY_BUFFER_SAMPLES         ( 160 )

/* Buffers used by DMAs .*/
static uint16_t DMARecordBuffer[ DMA_RECORD_BUFFER_SAMPLES ];
static uint16_t DMAPlayBuffer[ DMA_PLAY_BUFFER_SAMPLES ];

/* Flags to asynchronously close microphone/speaker. */
static bool openPlatformMicrophone = false;
static bool openPlatformSpeaker = false;
static bool enablePlatformTouchButton = false;

static uint32_t blinkInterval, blinkCount;

static void prvCapsenseIsr( void );
static void prvProcessTouch( void );
static void vCapsenseScanTask( void * p );

static void I2SIsrHandler( void );
static void DMARecordIsrHandler( void );
static void DMAPlayIsrHandler( void );
static void LEDBlinkyIsrHandler( void );

static void prvCapsenseIsr( void )
{
    Cy_CapSense_InterruptHandler( CYBSP_CSD_HW, &cy_capsense_context );
}

static void prvProcessTouch( void )
{
    uint32_t button1_status;
    static uint32_t button1_status_prev;

    /* Get button 1 status */
    button1_status = Cy_CapSense_IsSensorActive( CY_CAPSENSE_BUTTON1_WDGT_ID,
                                                 CY_CAPSENSE_BUTTON1_SNS0_ID,
                                                 &cy_capsense_context );

    /* Detect if button has been tapped, i.e. pressed and released */
    if( ( 0u != button1_status_prev ) &&
            ( 0u == button1_status ) )
    {
        vClientButtonTapped();
    }

    button1_status_prev = button1_status;
}

static void vCapsenseScanTask( void * p )
{
    Cy_CapSense_SetupWidget( CY_CAPSENSE_BUTTON1_WDGT_ID , &cy_capsense_context );
    Cy_CapSense_Scan( &cy_capsense_context );

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( CAPSENSE_SCAN_INTERVAL_MS ) );

        /* Check if CapSense is busy with a previous scan */
        if( CY_CAPSENSE_NOT_BUSY == Cy_CapSense_IsBusy( &cy_capsense_context )
                && enablePlatformTouchButton == true )
        {
            /* Process widget */
            Cy_CapSense_ProcessWidget( CY_CAPSENSE_BUTTON1_WDGT_ID, &cy_capsense_context );

            /* Process touch input */
            prvProcessTouch();

            /* Start next scan */
            Cy_CapSense_Scan( &cy_capsense_context );
        }
    }
}

BaseType_t xPlatformTouchButtonInit( void )
{
    cy_status status;
    BaseType_t xReturned = pdFAIL;

    /* CapSense interrupt configuration parameters */
    static const cy_stc_sysint_t capSense_intr_config =
    {
        .intrSrc = CYBSP_CSD_IRQ,
        .intrPriority = CAPSENSE_INTERRUPT_PRIORITY,
    };

    /*Initialize CapSense Data structures */
    status = Cy_CapSense_Init( &cy_capsense_context );
    if( CY_RET_SUCCESS == status )
    {
        /* Initialize CapSense interrupt */
        Cy_SysInt_Init( &capSense_intr_config, prvCapsenseIsr );
        NVIC_ClearPendingIRQ( capSense_intr_config.intrSrc );
        NVIC_EnableIRQ( capSense_intr_config.intrSrc );
    }

    if( CY_RET_SUCCESS == status )
    {
        /* Initialize the CapSense firmware modules. */
        status = Cy_CapSense_Enable( &cy_capsense_context );
    }

    if( CY_RET_SUCCESS == status )
    {
        xReturned = xTaskCreate( vCapsenseScanTask,
                                 "Capsense Scan",
                                 CAPSENSE_SCAN_TASK_STACK_SIZE,
                                 NULL,
                                 CAPSENSE_SCAN_TASK_PRIORITY,
                                 NULL );
    }

    return xReturned;

}

BaseType_t xPlatformLEDInit( void )
{
    /* GPIO init is done in init_cycfg_pins(). */
    vPlatformLEDOff();

    /* 1ms continuous timer for blinking. */
    NVIC_EnableIRQ( Cont_1ms_IRQ );
    cy_stc_sysint_t Cont_1ms_ISR_cfg =
    {
        .intrSrc = Cont_1ms_IRQ,
        .intrPriority = 5U,
    };
    Cy_SysInt_Init( &Cont_1ms_ISR_cfg, LEDBlinkyIsrHandler );
    Cy_TCPWM_Counter_Init( Cont_1ms_HW, Cont_1ms_NUM, &Cont_1ms_config );
    Cy_TCPWM_Counter_Enable( Cont_1ms_HW, Cont_1ms_NUM );

    return pdPASS;
}

BaseType_t xPlatformMicrophoneInit( void )
{
    Cy_PDM_PCM_Init( PDM_PCM_HW, &PDM_PCM_config );
    Cy_PDM_PCM_ClearFifo( PDM_PCM_HW );
    Cy_DMA_Descriptor_Init( &DMA_Record_Descriptor_0, &DMA_Record_Descriptor_0_config );
    Cy_DMA_Descriptor_SetSrcAddress( &DMA_Record_Descriptor_0, ( void * ) &PDM_PCM_HW->RX_FIFO_RD );
    Cy_DMA_Descriptor_SetDstAddress( &DMA_Record_Descriptor_0, ( void * ) DMARecordBuffer );
    Cy_DMA_Channel_Init( DMA_Record_HW, DMA_Record_CHANNEL, &DMA_Record_channelConfig );
    NVIC_EnableIRQ( DMA_Record_IRQ );
    cy_stc_sysint_t DMA_Record_ISR_cfg =
    {
        .intrSrc = DMA_Record_IRQ,
        .intrPriority = 5U,
    };
    Cy_SysInt_Init( &DMA_Record_ISR_cfg, DMARecordIsrHandler );
    Cy_DMA_Channel_SetInterruptMask( DMA_Record_HW, DMA_Record_CHANNEL, CY_DMA_INTR_MASK );
    Cy_DMA_Enable( DMA_Record_HW );

    return pdPASS;
}

BaseType_t xPlatformSpeakerInit( void )
{
    /* Initialize the I2S block */
    Cy_I2S_Init( I2S_HW, &I2S_config );
    Cy_I2S_ClearTxFifo( I2S_HW );
    NVIC_EnableIRQ( I2S_IRQ );
    cy_stc_sysint_t I2S_ISR_cfg =
    {
        .intrSrc = I2S_IRQ,
        .intrPriority = 5U,
    };
    Cy_SysInt_Init( &I2S_ISR_cfg, I2SIsrHandler );
    Cy_I2S_SetInterruptMask( I2S_HW, 0 );

    Cy_DMA_Descriptor_Init( &DMA_Play_Descriptor_0, &DMA_Play_Descriptor_0_config );
    Cy_DMA_Descriptor_SetSrcAddress( &DMA_Play_Descriptor_0, ( void * ) DMAPlayBuffer );
    Cy_DMA_Descriptor_SetDstAddress( &DMA_Play_Descriptor_0, ( void * ) &I2S_HW->TX_FIFO_WR );
    Cy_DMA_Channel_Init( DMA_Play_HW, DMA_Play_CHANNEL, &DMA_Play_channelConfig );
    NVIC_EnableIRQ( DMA_Play_IRQ );
    cy_stc_sysint_t DMA_Play_ISR_cfg =
    {
        .intrSrc = DMA_Play_IRQ,
        .intrPriority = 5U,
    };
    Cy_SysInt_Init( &DMA_Play_ISR_cfg, DMAPlayIsrHandler );

    Cy_DMA_Channel_SetInterruptMask( DMA_Play_HW, DMA_Play_CHANNEL,CY_DMA_INTR_MASK );
    Cy_DMA_Enable( DMA_Play_HW );

    return pdPASS;
}

/* Control functions. */

void vPlatformLEDOn( void )
{
    Cy_TCPWM_TriggerStopOrKill( Cont_1ms_HW, Cont_1ms_MASK );
    Cy_GPIO_Write( LED_PORT, LED_PIN, 0 );
}

void vPlatformLEDOff( void )
{
    Cy_TCPWM_TriggerStopOrKill( Cont_1ms_HW, Cont_1ms_MASK );
    Cy_GPIO_Write( LED_PORT, LED_PIN, 1 );
}

void vPlatformLEDBlink( uint32_t interval_ms )
{
    blinkCount = 0;
    blinkInterval = interval_ms / 2;
    Cy_TCPWM_TriggerStart( Cont_1ms_HW, Cont_1ms_MASK );
}

void vPlatformMicrophoneOpen( void )
{
    Cy_PDM_PCM_ClearFifo( PDM_PCM_HW );
    Cy_PDM_PCM_Enable( PDM_PCM_HW );
    Cy_DMA_Channel_Enable( DMA_Record_HW, DMA_Record_CHANNEL );
    openPlatformMicrophone = true;
}

void vPlatformMicrophoneClose( void )
{
    openPlatformMicrophone = false;
}

void vPlatformSpeakerOpen( void )
{
    Cy_I2S_ClearTxFifo( I2S_HW );
    Cy_I2S_EnableTx( I2S_HW );
    Cy_DMA_Channel_Enable( DMA_Play_HW, DMA_Play_CHANNEL );
    openPlatformSpeaker = true;
}

void vPlatformSpeakerClose( void )
{
    openPlatformSpeaker = false;
}

void vPlatformTouchButtonEnable( void )
{
    enablePlatformTouchButton = true;
}

void vPlatformTouchButtonDisable( void )
{
    enablePlatformTouchButton = false;
}

/* ISRs */

static void I2SIsrHandler( void )
{
    uint32_t interrupts = Cy_I2S_GetInterruptStatus( I2S_HW );
    if(( interrupts & CY_I2S_INTR_TX_EMPTY ) != 0 )
    {
        Cy_I2S_SetInterruptMask( I2S_HW, 0 );
        Cy_I2S_DisableTx( I2S_HW );
    }

    Cy_I2S_ClearInterrupt( I2S_HW, interrupts );
}

static void DMARecordIsrHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if( openPlatformMicrophone == false )
    {
        Cy_PDM_PCM_Disable( PDM_PCM_HW );
    }
    else
    {
        xClientFillMicrophoneBufferFromISR( DMARecordBuffer, sizeof( DMARecordBuffer ), &xHigherPriorityTaskWoken );

        Cy_DMA_Channel_Enable( DMA_Record_HW, DMA_Record_CHANNEL );
    }

    Cy_DMA_Channel_ClearInterrupt( DMA_Record_HW, DMA_Record_CHANNEL );

    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void DMAPlayIsrHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    size_t xDataSize;

    memset( DMAPlayBuffer, 0, sizeof( DMAPlayBuffer ) );
    xDataSize = xClientReadSpeakerBufferFromISR( DMAPlayBuffer, sizeof( DMAPlayBuffer ), &xHigherPriorityTaskWoken );

    /* Stop the DMA when the close flag is set and no data remained in the speaker buffer */
    if( openPlatformSpeaker == false && xDataSize == 0 )
    {
        /* Enable CY_I2S_INTR_TX_EMPTY to disable I2S after the current data in the FIFOs are played. */
        Cy_I2S_SetInterruptMask( I2S_HW, CY_I2S_INTR_TX_EMPTY );
    }
    else
    {
        Cy_DMA_Channel_Enable( DMA_Play_HW, DMA_Play_CHANNEL );
    }

    Cy_DMA_Channel_ClearInterrupt( DMA_Play_HW, DMA_Play_CHANNEL );

    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void LEDBlinkyIsrHandler( void )
{
    blinkCount++;
    if( blinkCount == blinkInterval )
    {
        Cy_GPIO_Inv( LED_PORT, LED_NUM );
        blinkCount = 0;
    }
    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked( Cont_1ms_HW, Cont_1ms_NUM );
    Cy_TCPWM_ClearInterrupt( Cont_1ms_HW, Cont_1ms_NUM, interrupts );
}
