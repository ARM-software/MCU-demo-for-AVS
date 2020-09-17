#!/usr/bin/env bash

# Copyright (C) 2019 - 2020 Arm Ltd.  All Rights Reserved.
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

set -e

# CUSTOM INFORMATION

# MANDATORY
AWS_ACCOUNT=""
PRODUCT_ID=""
CLIENT_ID=""
IOT_THING_NAME=""
IOT_ENDPOINT=""

# OPTIONAL
TOPIC_ROOT=""
PUBLIC_KEY=""
SERIAL_NUMBER=""

# END OF CUSTOM INFORMATION


function usage()
{
    cat << EOF
Generate credentials required for AIA demo. Use -h to show the usage.

Dependencies:

    This script depends on OpenSSL 1.1.0 or later versions to generate X25519 keys.
    The latest OpenSSL package on Ubuntu 16.04 is lower than the required version
    thus you'll have to use the script on a newer Ubuntu distribution. Or you could
    generate your own X25519 key pair and specify the public key encoded in base64
    in the PUBLIC_KEY variable in the script and proceed with the authorization.
    The script has also been tested in Git Bash 2.28.0 on Windows 10, which comes
    along with the mingw64 OpenSSL 1.1.1 package.

Usage:
    $0 [-h]

    First fill in the variables in the #CUSTOM INFORMATION section at the beginning
    of this script. Then run the script without any arguments. Follow the
    instructions shown on the console to verify the given user code in the given
    link. Hit ENTER after the verification completes and you will be able to get
    your own public/private key pair as well as the public key from the server.

Descriptions of custom variables:

    Mandatory:
        AWS_ACCOUNT:    Your AWS developer account ID.
        PRODUCT_ID:     The Product ID of your Alexa Voice Service product, which
                        can be found in your Alexa Voice Service Developer Console
                        at https://developer.amazon.com/alexa/console/avs/products.
                        The Product ID is right next to the name of your product.
        CLIENT_ID:      The Client ID for your product which can also be found in
                        the AVS Developer Console. Click on your product. Under
                        "Product Details", click "Security Profile". Select the
                        "Other devices and platforms" tab on the right hand and
                        find your Client ID there.
        IOT_THING_NAME: The name of your IoT thing registered with AWS IoT, which
                        can be found in your AWS IoT console. Note that the AWS IoT
                        console is different from the AVS Developer Console. The
                        specific URL of your AWS IoT console depends on the region
                        you choose. In the console, choose "Manage->Things" from
                        the navigation pane to get a list of your Things.
        IOT_ENDPOINT:   The AWS IoT endpoint your IoT thing connects to, which can
                        be found in your AWS IoT console in "Settings" from the
                        navigation pane.

    Optional:
        TOPIC_ROOT:     The MQTT topic path that precedes AIA-specific topics.
                        Leave it empty if you did not specify your own in your AWS
                        IoT rules.
        PUBLIC_KEY:     The script depends on OpenSSL 1.1.0 or higher to generate
                        X25519 keys. You can generate your key pair with other methods
                        and specify your base64-encoded public key in this variable.
        SERIAL_NUMBER:  A key that uniquely identifies this instance of your product.
                        For example, this could be a serial number or MAC address. If
                        not specified 12345 will be used by default.
EOF
}

function send_request()
{
    local cmd="$1"
    local error_message="$2"

    res=$(eval $cmd)
    http_code=${res##*;}
    res=${res%;*}
    if [ "$http_code" != 200 ]; then
        echo "$error_message" >&2
        echo "Failure response: $res" >&2
        exit 1
    else
        echo "$res"
    fi
}

while getopts ":h" opt; do
    case ${opt} in
        h )
            usage
            exit 0
            ;;
        \? )
            echo "Invalid option: ${OPTARG}." >&2
            usage
            exit 1
            ;;
    esac
done

[ -z "$CLIENT_ID" ] && echo "Please specify client ID. Use -h to show the usage." >&2 && exit 1
[ -z "$PRODUCT_ID" ] && echo "Please specify product ID. Use -h to show the usage." >&2 && exit 1
[ -z "$AWS_ACCOUNT" ] && echo "Please specify AWS account. Use -h to show the usage." >&2 && exit 1
[ -z "$IOT_THING_NAME" ] && echo "Please specify IoT thing name. Use -h to show the usage." >&2 && exit 1
[ -z "$IOT_ENDPOINT" ] && echo "Please specify IoT endpoint. Use -h to show the usage." >&2 && exit 1

SERIAL_NUMBER=${SERIAL_NUMBER:-12345}

# Append the http response code to the curl response.
CURL_CMD="curl -s -w \";%{http_code}\" -X POST"

# Device Authorization Request
RESPONSE_TYPE="device_code"
SCOPE="alexa:all"
HEADER="Content-Type: application/x-www-form-urlencoded"
ENDPOINT="https://api.amazon.com/auth/O2/create/codepair"
CMD="${CURL_CMD} -d 'response_type=${RESPONSE_TYPE}&client_id=${CLIENT_ID}&scope=${SCOPE}&scope_data={\"${SCOPE}\":{\"productID\":\"${PRODUCT_ID}\",\"productInstanceAttributes\":{\"deviceSerialNumber\":\"${SERIAL_NUMBER}\"}}}' -H \"${HEADER}\" ${ENDPOINT}"

res=$(send_request "$CMD" "Failed to request device authorization.")

USER_CODE=$(echo "$res" | tr -d "{}" | cut -d "," -f 1 | cut -d ":" -f 2 | tr -d '"')
VERIFICATION_URI=$(echo "$res" | tr -d "{}" | cut -d "," -f 4 | cut -d ":" -f 2- | tr -d '"')
DEVICE_CODE=$(echo "$res" | tr -d "{}" | cut -d "," -f 2 | cut -d ":" -f 2 | tr -d '"')

echo -e "\nPlease navigate to $VERIFICATION_URI, login and enter the user code: $USER_CODE\n"
read -p "After you have finished registration on the above link, hit ENTER to continue..."
echo

# Device Token Request
GRANT_TYPE="device_code"
HEADER="POST /auth/o2/token Host: api.amazon.com Content-Type: application/x-www-form-urlencoded"
ENDPOINT="https://api.amazon.com/auth/O2/token"
CMD="${CURL_CMD} -d 'grant_type=${GRANT_TYPE}&device_code=${DEVICE_CODE}&user_code=${USER_CODE}' -H \"${HEADER}\" ${ENDPOINT}"

res=$(send_request "$CMD" "Failed to request device token.")

ACCESS_TOKEN=$(echo "$res" | tr -d "{}" | cut -d "," -f 1 | cut -d ":" -f 2 | tr -d '"')
REFRESH_TOKEN=$(echo "$res" | tr -d "{}" | cut -d "," -f 2 | cut -d ":" -f 2 | tr -d '"')

# Register AIA Device
HEADER="Content-Type: application/json"
ENDPOINT="https://api.amazonalexa.com/v1/ais/registration"

# If no PUBLIC_KEY is given, generate X25519 key pair with openssl.
if [ -z "$PUBLIC_KEY" ]; then
    KEY_PAIR=$(openssl genpkey -algorithm X25519 -text)
    PRIVATE_KEY=$(echo "$KEY_PAIR" | tr -d [:space:] | sed 's/.*priv:\(.*\)pub:.*/\1/' | sed 's/^\|:/\\x/g')
    PUBLIC_KEY=$(echo "$KEY_PAIR" | tr -d [:space:] | sed 's/.*pub:\(.*\)/\1/' | sed 's/^\|:/\\x/g')
    PRIVATE_KEY=$(echo -ne "$PRIVATE_KEY" | base64)
    PUBLIC_KEY=$(echo -ne "$PUBLIC_KEY" | base64)
fi

CMD="${CURL_CMD} --http1.1 -d '{\"authentication\":{\"token\":\"${REFRESH_TOKEN}\",\"clientId\":\"${CLIENT_ID}\"},\"encryption\":{\"algorithm\":\"ECDH_CURVE_25519_32_BYTE\",\"publicKey\":\"${PUBLIC_KEY}\"},\"iot\":{\"awsAccountId\":\"${AWS_ACCOUNT}\",\"clientId\":\"${IOT_THING_NAME}\",\"endpoint\":\"${IOT_ENDPOINT}\""

# If TOPIC_ROOT is given, include it in the request.
if [ -n "$TOPIC_ROOT" ]; then
    CMD="${CMD},\"topicRoot\":\"${TOPIC_ROOT}\""
fi

CMD="${CMD}}}' -H \"${HEADER}\" ${ENDPOINT}"

res=$(send_request "$CMD" "Failed to register AIA device.")

PEER_PUBLIC_KEY=$(echo "$res" | cut -d "," -f 1 | cut -d ":" -f 3 | tr -d ' {}"')
TOPIC_ROOT=$(echo "$res" | cut -d "," -f 2 | cut -d ":" -f 3 | tr -d ' {}"')

echo "Your public key:    ${PUBLIC_KEY}"
echo "Your private key:   ${PRIVATE_KEY}"
echo "Peer public key:    ${PEER_PUBLIC_KEY}"
echo "Topic root:         ${TOPIC_ROOT}"
