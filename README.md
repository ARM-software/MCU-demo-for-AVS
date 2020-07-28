# MCU demo for AVS

Typical Alexa smart speakers require high device-side compute and memory to handle interactions with the [Alexa Voice Service](https://developer.amazon.com/en-US/alexa/alexa-voice-service). Amazon introduced the [Alexa Voice Service Integration for AWS IoT Core](https://docs.aws.amazon.com/iot/latest/developerguide/avs-integration-aws-iot.html) (AIA) to offload much of the complexity to an Amazon-managed cloud service. This enables support for Alexa on constrained devices based on microcontrollers with on-chip memory. Arm undertook an effort to understand the compute and memory requirements by implementing an AIA application.

This repository includes an Arm-provided example implementation of the AIA client on top of [Amazon FreeRTOS](https://aws.amazon.com/freertos/) for use with [Arm Cortex-M Series Processors](https://developer.arm.com/ip-products/processors/cortex-m). The implementation is compliant with [AWS MQTT (v2.0.0) C SDK API](https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/mqtt/index.html). The repository also includes a demonstration of how the AIA client can be used on Cypress PSoC 6 Wi-Fi
BT Prototyping Kit (CY8CPROTO-062-4343W) based on [Cypress Amazon FreeRTOS Project](https://github.com/cypresssemiconductorco/amazon-freertos).

# Contents of this repository
`aia/`: contains the reference AIA client implementation. It also contains a header file `aia_platform.h`, which describes the platform specific APIs that need to be implemented in order to run the AIA application.  
`demo/`: contains reference files for running the demo on CY8CPROTO-062-4343W. Currently it contains two files: `aia_demo.c` which provides an entry point for the AIA demo in the Amazon FreeRTOS project; `aia_platform.c` which gives a reference design for platform specific APIs required by the AIA client.  
`patch/CY8CPROTO-062-4343W.patch`: a patch file generated aganist a release version of Cypress Amazon FreeRTOS which contains project-specific configurations. It can be applied to set up an AIA demo project that runs on CY8CPROTO-062-4343W board. Details can be found in the **Set up the demo on CY8CPROTO-062-4343W** section.  
`LICENSE`: MIT license.  
`README.md`: this file.

# Set up the demo on CY8CPROTO-062-4343W

## Hardware setup
The demo requires an external audio output module, e.g. [Digilent Pmod I2S2](https://store.digilentinc.com/pmod-i2s2-stereo-audio-input-and-output/), as a speaker. A bit of hardware rework to the CY8CPROTO-062-4343W board needs to be done to use Pmod I2S2 for the demo.

 1. Remove R72, R73, R74, R75 because the same pins are used for UART by default.
 2. Connect P0.5 (J1.9) to P5.0 (J2.40 or J5.1) with a wire to provide a MCLK for Pmod I2S2. Pmod I2S2 requires a MCLK to work properly. In this demo we output a MCLK clock signal on P0.5 generated from the same clock souce as SCLK/LRCK to keep them synchronized.
 3. As the default UART pins have been occupied by Pmod I2S2, in this demo we use another pair of UART RX/TX signals P12.0/P12.1 for serial communication. To do so, connect UART_TX/UART_RX pins (J6.5/J6.4) on KitProg3 Section to P12.0/P12.1 (J1.18/J1.19) respectively. Do not mix them up.

After you have tweaked your board, plug a USB cable into the KitProg3 USB connector (J8) to connect the board to your host computer and you are all set to go.

## Project setup
 1. Download [Cypress amazon-freertos 201908.00 release](https://github.com/cypresssemiconductorco/amazon-freertos/releases/tag/201908-MTBAFR1941) and extract it to your local filesystem.
 2. From the top-level folder `amazon-freertos-xxx`, create a `demos/aia` folder.
 3. Copy the files in the `aia/` folder and the `demo/` folder in this repository to the `demos/aia` folder just created.
 4. Download [opus 1.3.1 source code](https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz), extract it from the top-level folder with the following command:
    ```
    tar zxvf opus-1.3.1.tar.gz --one-top-level=libraries/3rdparty/opus --strip-components 1
    ```
 5. Apply `patch/CY8CPROTO-062-4343W.patch` from the top-level folder.
 6. Open [ModusToolbox IDE](https://www.cypress.com/products/modustoolbox-software-environment), import the project from `projects/cypress/CY8CPROTO_062_4343W/mtb/aws_demos/`
 7. Launch the Device Configurator from the Quick Panel in ModusToolbox, do nothing but save and exit. This is for the Device Configurator to automatically generate platform configuration source code based on the design.modus and design.cycapsense files we made for this demo.
 8. Fill in your credentials information. See the next section **Credentials**.

## Credentials
 1. You should have set up your AWS account and registered your device with AWS IoT before running the demo. Refer to [First Steps to Get Started with Amazon FreeRTOS](https://docs.aws.amazon.com/freertos/latest/userguide/freertos-prereqs.html) to generate required credentials. Fill in these credentials as well as your Wi-Fi configurations in aws_clientcredential.h and aws_clientcredential_keys.h as instructed.
 2. Specify your AWS account ID for the `aiaconfigAWS_ACCOUNT_ID` macro which can be found in `demos/aia/aia_client_config.h`.
 3. AIA and the end device require a shared secret for encrypting and decrypting messages. You must register to the secure HTTP/1.1 endpoint exposed by AIA after you have completed a standard AVS LWA authentication flow. Refer to [Registration](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-registration.html) for more detailed information. Since the current AIA client reference design in this repository does not include a HTTP/1.1 protocol implementation, you need to make this registration on your host computer with tools capable of HTTP transactions, e.g. **curl** (Example usage: `curl --http1.1 -H <header> -d <data> <URL>`). Before you do that, you also need to generate a pair of public/private keys using AES256-GCM and provide the public key during the AIA registration process. After the registration succeeds, you will get the server-side public key. Using your public/private keys and the server-side public key, the application is able to deduce the shared secret for message encryption/decryption. Specify these three keys for the following macros that can be found in `demos/aia/aia_client_config.h`:
    ```
    aiaconfigCLIENT_PUBLIC_KEY
    aiaconfigCLIENT_PRIVATE_KEY
    aiaconfigPEER_PUBLIC_KEY
    ```
 4. You will also get the root topic for AIA-specific topics if the registration is successful. You can find its value in the "topicRoot" field from the HTTP success response. Fill in the `aiaconfigTOPIC_ROOT` macro in `demos/aia/aia_client_config.h` with this value.

## Run the demo
Build the project and program the board, you should be able to run the demo.
You are able to see logs from the console if you have set up the UART correctly as described in the **Hardware setup** section. You will see the device connect to your MQTT endpoint and AIA service if things go well, and the device will wait in IDLE state for your action. You can also tell the status of the device from the user LED on the board if you do not have a UART connection. The LED will start blinking after the MQTT connection is established and stay on after entering IDLE state. You can now tap the capsense button BTN1 in the bottom right corner to start a conversion with AIA.

## Known issues
The lwIP library includes a header file 'api.h', while the Opus library includes 'API.h'. It's not an issue on Linux hosts. However, since Windows is case insensitive in terms of file systems, the user needs to specify the path of these two header files in the source files that include them, to ensure the correct one is included.

# License

The software is provided under the [MIT](https://spdx.org/licenses/MIT.html) license. See [LICENSE](LICENSE) for more information.

# Maintenance and issues

Maintenance for this example will be limited. Please feel free to raise an [issue on GitHub](https://github.com/ARM-software/MCU-demo-for-AVS/issues) to report bugs or start discussions about enhancements.
