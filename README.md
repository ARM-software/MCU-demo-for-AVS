# MCU demo for AVS

Typical Alexa smart speakers require high device-side compute and memory to handle interactions with the [Alexa Voice Service](https://developer.amazon.com/en-US/alexa/alexa-voice-service) (AVS). Amazon introduced the [Alexa Voice Service Integration for AWS IoT Core](https://docs.aws.amazon.com/iot/latest/developerguide/avs-integration-aws-iot.html) (AIA) to offload much of the complexity to an Amazon-managed cloud service. This enables support for Alexa on constrained devices based on microcontrollers with on-chip memory. Arm undertook an effort to understand the compute and memory requirements by implementing an AIA application.

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
 1. You should have set up your AWS account and registered your device with AWS IoT before running the demo. Refer to [First Steps to Get Started with Amazon FreeRTOS](https://docs.aws.amazon.com/freertos/latest/userguide/freertos-prereqs.html) to generate required credentials. Select a region closest to your location. Skip the steps regarding "Quick Connect" or "Downloading FreeRTOS". Fill in these credentials as well as your Wi-Fi configurations in aws_clientcredential.h and aws_clientcredential_keys.h as instructed.
 2. Specify your AWS account ID for the `aiaconfigAWS_ACCOUNT_ID` macro which can be found in `demos/aia/aia_client_config.h`.
 3. Apart from your AWS account, you also need an Amazon developer account to access AVS. Follow this [page](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/register-a-product.html) to create your developer account and register an AVS product, with a few deviations from the instructions in the [Fill in product information](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/register-a-product.html#fill-in-product-information) section:
- In step 5, choose "Smart Home" for "Product category".
- In step 7, choose "Touch-initiated" for "How will users interact with your product".
- In step 11, choose "Yes" for "Is this device associated with one or more AWS IoT Core Accounts?" and specify your AWS Account ID.
 4. AIA and the end device use a shared secret for encrypting and decrypting messages. A key exchange is required so that we can retrieve a public key from AIA to generate the shared secret. Details can be found in [Registration](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-registration.html).  
 At this point, this demo does not support on-device registration. It uses **Code Based Linking** for LWA authentication and the whole registration process is done on a host with HTTP capability. The user private/public key pair as well as the AIA public key are hard coded into the source code before building the demo.  
 To make things easier, a [gen_credentials.sh](gen_credentials.sh) bash script is provided to automate the whole process as much as possible. Just fill in a few variables with your custom information at the beginning of the script before you run it in a terminal. Follow the printed instructions and you will get all the credentials you need in seconds. Use '-h' option with the script to see the usage and the dependencies, as well as the descriptions of each required variable and how to get them.
 5. After a successful run of the script, you will get four items: your public key, private key, peer(AIA) public key and the topic root. Specify these four values for the four macros respectively in `demos/aia/aia_client_config.h`:
    ```
    aiaconfigCLIENT_PUBLIC_KEY
    aiaconfigCLIENT_PRIVATE_KEY
    aiaconfigPEER_PUBLIC_KEY
    aiaconfigTOPIC_ROOT
    ```

## Run the demo
Build the project and program the board, you should be able to run the demo.
You are able to see logs from the console if you have set up the UART correctly as described in the **Hardware setup** section. You will see the device connect to your MQTT endpoint and AIA service if things go well, and the device will wait in IDLE state for your action. You can also tell the status of the device from the user LED on the board if you do not have a UART connection. The LED will start blinking after the MQTT connection is established and stay on after entering IDLE state. You can now tap the capsense button BTN1 in the bottom right corner to start a conversion with AIA.

## Known issues
- The lwIP library includes a header file 'api.h', while the Opus library includes 'API.h'. It's not an issue on Linux hosts. However, since Windows and macOS(by default) are case insensitive in terms of file systems, the user needs to specify the path of these two header files in the source files that include them, to ensure the correct one is included.
Please apply `opus_WINDOWS_MAC.patch` in `patch/` folder in this repository if you are a Windows or macOS user.
- The default GCC compiler used by the Cypress amazon-freertos project is gcc-7.2.1 under ModusToolbox installation location. If the compiler path does not match that installed by your ModusToolbox IDE, you might fail to build the project. For example, for macOS users, you might encounter the following build error with ModusToolbox 2.2.0:
    ```
    make: /Applications/ModusToolbox/tools_2.2/gcc-7.2.1/bin/arm-none-eabi-gcc: No such file or directory
    ```
  In such case, please specify the correct gcc path by either:
  - setting CY_COMPILER_DEFAULT_DIR in `vendors/cypress/psoc6/psoc6make/make/core/main.mk`.
  - setting CY_COMPILER_DIR and CY_COMPILER_GCC_ARM_DIR in `projects/cypress/CY8CPROTO_062_4343W/mtb/aws_demos/Makefile`.

# License

The software is provided under the [MIT](https://spdx.org/licenses/MIT.html) license. See [LICENSE](LICENSE) for more information.

# Maintenance and issues

Maintenance for this example will be limited. Please feel free to raise an [issue on GitHub](https://github.com/ARM-software/MCU-demo-for-AVS/issues) to report bugs or start discussions about enhancements.
