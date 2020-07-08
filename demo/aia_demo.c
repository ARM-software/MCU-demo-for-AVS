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

#include "aia_client.h"

#define MQTT_KEEP_ALIVE_INTERVAL_SECONDS        ( 1200UL )
#define MQTT_TIMEOUT_MILLISECONDS               ( 5000UL )

IotMqttConnection_t xMqttConnection;

static BaseType_t prvEstablishMqttConnection( bool awsIotMqttMode,
                                              const char * pIdentifier,
                                              void * pNetworkServerInfo,
                                              void * pNetworkCredentialInfo,
                                              const IotNetworkInterface_t * pNetworkInterface )
{
    BaseType_t xReturned = pdPASS;
    IotMqttError_t xConnectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t xNetworkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t xConnectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    IotNetworkServerInfo_t * pxServerInfo = ( IotNetworkServerInfo_t * )pNetworkServerInfo;

    /* Set the members of the network info not set by the initializer. This
     * struct provided information on the transport layer to the MQTT connection. */
    xNetworkInfo.createNetworkConnection = true;
    xNetworkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
    xNetworkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
    xNetworkInfo.pNetworkInterface = pNetworkInterface;

    /* Set the members of the connection info not set by the initializer. */
    xConnectInfo.awsIotMqttMode = awsIotMqttMode;
    xConnectInfo.cleanSession = true;
    xConnectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_SECONDS;
    xConnectInfo.pClientIdentifier = pIdentifier;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( pIdentifier );

    configPRINTF( ( "AIAClient %s attempts to connect to %s (port %hu)\r\n",
                    pIdentifier,
                    pxServerInfo->pHostName,
                    pxServerInfo->port ) );

    xConnectStatus = IotMqtt_Connect( &xNetworkInfo,
                                      &xConnectInfo,
                                      MQTT_TIMEOUT_MILLISECONDS,
                                      &xMqttConnection );

    if( xConnectStatus != IOT_MQTT_SUCCESS )
    {
        configPRINTF( ( "MQTT CONNECT returned error %s.", IotMqtt_strerror( xConnectStatus ) ) );

        xReturned = pdFAIL;
    }

    return xReturned;
}

/**
 * @brief The function that runs the AIA demo, called by the demo runner.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 *
 * @return `EXIT_SUCCESS` if the demo completes successfully; `EXIT_FAILURE` otherwise.
 */
int RunAIADemo( bool awsIotMqttMode,
                const char * pIdentifier,
                void * pNetworkServerInfo,
                void * pNetworkCredentialInfo,
                const IotNetworkInterface_t * pNetworkInterface )
{
    /* Return value of this function and the exit status of this program. */
    int status = EXIT_SUCCESS;

    if( prvEstablishMqttConnection( awsIotMqttMode,
                                    pIdentifier,
                                    pNetworkServerInfo,
                                    pNetworkCredentialInfo,
                                    pNetworkInterface ) == pdFAIL )
    {
        goto aia_task_exit;
    }

    if( xClientInit( xMqttConnection ) == pdFAIL )
    {
        goto aia_task_exit;
    }

    xDemoTaskHandle = xTaskGetCurrentTaskHandle();

    if( xClientAIAInit() == pdFAIL )
    {
        goto aia_task_exit;
    }

    /* The demo task waits for the termination signal from AIA client. */
    xTaskNotifyWait( 0, 0, NULL, portMAX_DELAY );

aia_task_exit:
    vTaskDelay(100);
    vClientCleanup();

    /* The demo only ends on failure. */
    status = EXIT_FAILURE;
    return status;
}
