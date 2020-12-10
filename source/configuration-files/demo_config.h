/*
 * FreeRTOS Kernel V10.3.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 */

#ifndef DEMO_CONFIG_H
#define DEMO_CONFIG_H

/**************************************************/
/******* DO NOT CHANGE the following order ********/
/**************************************************/

/* Include logging header files and define logging macros in the following order:
 * 1. Include the header file "logging_levels.h".
 * 2. Define the LIBRARY_LOG_NAME and LIBRARY_LOG_LEVEL macros depending on
 * the logging configuration for DEMO.
 * 3. Include the header file "logging_stack.h", if logging is enabled for DEMO.
 */

#include "logging_levels.h"

/* Logging configuration for the Demo. */
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "MQTTDemo"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_DEBUG
#endif

/* Prototype for the function used to print to console on Windows simulator
 * of FreeRTOS.
 * The function prints to the console before the network is connected;
 * then a UDP port after the network has connected. */
extern void vLoggingPrintf( const char * pcFormatString,
                            ... );

/* Map the SdkLog macro to the logging function to enable logging
 * on Windows simulator. */
#ifndef SdkLog
    #define SdkLog( message )    vLoggingPrintf message
#endif

#include "logging_stack.h"

/************ End of logging configuration ****************/

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique so edit as required to ensure no two clients connecting to the
 * same broker use the same client identifier.
 *
 *!!! Please note a #defined constant is used for convenience of demonstration
 *!!! only.  Production devices can use something unique to the device that can
 *!!! be read by software, such as a production serial number, instead of a
 *!!! hard coded constant.
 *
 * #define democonfigCLIENT_IDENTIFIER				"insert here."
 */
#define democonfigCLIENT_IDENTIFIER    "Thing3"

/**
 * @brief Endpoint of the MQTT broker to connect to.
 *
 * This demo application can be run with any MQTT broker, although it is
 * recommended to use one that supports mutual authentication. If mutual
 * authentication is not used, then #democonfigUSE_TLS should be set to 0.
 *
 * For AWS IoT MQTT broker, this is the Thing's REST API Endpoint.
 *
 * @note Your AWS IoT Core endpoint can be found in the AWS IoT console under
 * Settings/Custom Endpoint, or using the describe-endpoint REST API (with
 * AWS CLI command line tool).
 *
 * #define democonfigMQTT_BROKER_ENDPOINT				"insert here."
 */
//#define democonfigMQTT_BROKER_ENDPOINT                  "test.mosquitto.org"
//#define democonfigMQTT_BROKER_ENDPOINT                  "192.168.0.25"
#define democonfigMQTT_BROKER_ENDPOINT                  "a91t51ne5wq71-ats.iot.us-west-2.amazonaws.com"

/**
 * @brief The port to use for the demo.
 *
 * In general, port 8883 is for secured MQTT connections, and port 1883 if not
 * using TLS.
 *
 * @note Port 443 requires use of the ALPN TLS extension with the ALPN protocol
 * name. Using ALPN with this demo would require additional changes, including
 * setting the `pAlpnProtos` member of the `NetworkCredentials_t` struct before
 * forming the TLS connection. When using port 8883, ALPN is not required.
 *
 * #define democonfigMQTT_BROKER_PORT    ( insert here. )
 */
//#define democonfigMQTT_BROKER_PORT ( 8883 )

/**
 * @brief Server's root CA certificate.
 *
 * For AWS IoT MQTT broker, this certificate is used to identify the AWS IoT
 * server and is publicly available. Refer to the AWS documentation available
 * in the link below.
 * https://docs.aws.amazon.com/iot/latest/developerguide/server-authentication.html#server-authentication-certs
 *
 * @note This certificate should be PEM-encoded.
 *
 * @note If you would like to setup an MQTT broker for running this demo,
 * please see `mqtt_broker_setup.txt`.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define democonfigROOT_CA_PEM    "...insert here..."
 */
#define democonfigROOT_CA_PEM                                             \
    "-----BEGIN CERTIFICATE-----\n"                                      \
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
    "rqXRfboQnoZsG4q5WTP468SQvvG5\n"                                     \
    "-----END CERTIFICATE-----\n"

/**
 * @brief Client certificate.
 *
 * For AWS IoT MQTT broker, refer to the AWS documentation below for details
 * regarding client authentication.
 * https://docs.aws.amazon.com/iot/latest/developerguide/client-authentication.html
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define democonfigCLIENT_CERTIFICATE_PEM    "...insert here..."
 */
#define democonfigCLIENT_CERTIFICATE_PEM                                  \
"-----BEGIN CERTIFICATE-----\n"\
"MIIDWTCCAkGgAwIBAgIUJgGp2otBnSu1zlXmgjV7wH3r/5MwDQYJKoZIhvcNAQEL\n"\
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n"\
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTE5MTAxNTE3MzY0\n"\
"OFoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n"\
"ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANJ6ZWn6z8lluVvDcBjY\n"\
"IonMX1XPMiSfNKXqbBKw6J3xoGJk4gcMKKw8C49WWtFDnGHa3UVROUx4w9jDrWfE\n"\
"eHZy9HhLOU9GCfQU2fwiUaQnQNXCceoUPZdl6V6L3hyEJBcDMvLalTX88O8hYJCQ\n"\
"L4miVLlMEMZecWZbzd9lNFboOUCd8JZ2wn4krLF9VX7+GRmoRN2Gi9hQmIn2OVIY\n"\
"WXK2B8V468wzI3O78eq2X3L1RGNvWTgyoGIpOXngxLD4i9nQK9Fleu9IHoTDmjz5\n"\
"OdAUpWeO9QHv8Bq8MgnzNHg59jlQyAHJyUDyDA0cNeB+g+/DIs8gFVo74Q9VJLiT\n"\
"y+sCAwEAAaNgMF4wHwYDVR0jBBgwFoAU3LqZ+L5sHMDAqQnu7jYFmoeeVvYwHQYD\n"\
"VR0OBBYEFPF7lsAdltfelSVVUY1tCdKm7/PnMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n"\
"AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQBAXRmoPT0lAPUdtsZzkxBY/rQd\n"\
"ooG8weu1jw7WIGd7OcU017pB0YWIy3CcEdCs+WrJpwzwgJ3I9SqmVx8w/MYx8cFa\n"\
"388aetA+WQS5GqnmxWCgVIEMcrRBqLCkpq397u99gKm130BLQT285ulpVOnHEzTE\n"\
"xrHsaF96gWnvXA+lO2e45gknWRZgt5c+UNzpR7qd0DooqLVlfj8y8gUhaD4XdgH2\n"\
"n/bD2r9ZaPGLevOyhk1CboS56qpysmn3OwjMEm+GIvmODZx7x6Mm/9O7TC0ky9IH\n"\
"kNbOKbGlTTxzAZoJ1NjnfVqRCOvxi4aOOC3bz/cejQM1ezPoiiBsJHJxHsQC\n"\
"-----END CERTIFICATE-----"




/**
 * @brief Client's private key.
 *
 *!!! Please note pasting a key into the header file in this manner is for
 *!!! convenience of demonstration only and should not be done in production.
 *!!! Never paste a production private key here!.  Production devices should
 *!!! store keys securely, such as within a secure element.  Additionally,
 *!!! we provide the corePKCS library that further enhances security by
 *!!! enabling securely stored keys to be used without exposing them to
 *!!! software.
 *
 * For AWS IoT MQTT broker, refer to the AWS documentation below for details
 * regarding clientauthentication.
 * https://docs.aws.amazon.com/iot/latest/developerguide/client-authentication.html
 *
 * @note This private key should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN RSA PRIVATE KEY-----\n"\
 * "...base64 data...\n"\
 * "-----END RSA PRIVATE KEY-----\n"
 *
 * #define democonfigCLIENT_PRIVATE_KEY_PEM    "...insert here..."
 */
#define democonfigCLIENT_PRIVATE_KEY_PEM                                  \
"-----BEGIN RSA PRIVATE KEY-----\n"\
"MIIEpQIBAAKCAQEA0nplafrPyWW5W8NwGNgiicxfVc8yJJ80pepsErDonfGgYmTi\n"\
"BwworDwLj1Za0UOcYdrdRVE5THjD2MOtZ8R4dnL0eEs5T0YJ9BTZ/CJRpCdA1cJx\n"\
"6hQ9l2XpXoveHIQkFwMy8tqVNfzw7yFgkJAviaJUuUwQxl5xZlvN32U0Vug5QJ3w\n"\
"lnbCfiSssX1Vfv4ZGahE3YaL2FCYifY5UhhZcrYHxXjrzDMjc7vx6rZfcvVEY29Z\n"\
"ODKgYik5eeDEsPiL2dAr0WV670gehMOaPPk50BSlZ471Ae/wGrwyCfM0eDn2OVDI\n"\
"AcnJQPIMDRw14H6D78MizyAVWjvhD1UkuJPL6wIDAQABAoIBAQC4lG/VZgfM5bGN\n"\
"ALKQhxKa56h/dwnRRfEEw7TtG0mUIW72euQhLA+LI8k7dY6FUBaXVjmP7XAjWRDf\n"\
"SpMKmijOL3em+skSdSiLbbHQxEP2ghoAm9oMXp245L2olV4+gb+okryebwRFaUHd\n"\
"Y9bsUBXwTHSiX0uiuvYVvtKnY3hn8Py2DxYElxwlGzHQ1fVkOaXDiRSyeUprcP6u\n"\
"iCzGZRjtq92Y0XSErQr3w2PnJdq9MRXk5l592HD1L5SRj90WH/jRMbPJY005hNd2\n"\
"wl6XR4p0FYtpXRGeFzbnrFhwudW+Tax0ERIo+LqU0YSyqD/p4NTsVXWptSGt8Iau\n"\
"maHjYq55AoGBAPumH3NzRLJ5ZXYEbZPSEPbGTYZVnad7WAf8TjDaMmzdm/PLtg1J\n"\
"g/NxLLlL4bYAsQ7Pg9p97MRsE5h7OUhytJ9DbrshuXRNMY/TlHW2+T9dGsusaV1L\n"\
"uH8WpNqtJCKB5pr9s6akOvbKX3ov3Ec+Vs5bYv2B/+hMXfunPvQ27SLlAoGBANYe\n"\
"CdKF0kLrwcnajdDRtZ7hIUY3/cmYHytSx9E96JvPT6X2DPJSjbFrrtfvCi0K/+Fm\n"\
"svNqQbsijtSeec7zODf3WiTo7BkI1rybxM8TCghkhHcNOTRwWQNmxjlgSC8bGSGd\n"\
"F9Jy9OlTETBa6wyUMS80/8cWnqhX7UvT0O6wrTaPAoGAaProa/VOV1YlaZJ3VA9y\n"\
"XEBl3wCggFoIY2xyAhdEqf3ZLV9yVyCwF0LDcZmiU5b+RjtzuhaGS3r6wcXGI98W\n"\
"UsqCyzZKc6YwYtvVNzZZzIE+yHDok68fDIWZyFAqnuqqFUZ5R6+DmajbI9ILhv0O\n"\
"oY+mQDOXWoVhP7aJoL+5NbECgYEAqWJH+O/+fwxMWKf5ynkryY1lqkv/C+y4s6gg\n"\
"BMqJ6kCdTLgSU9y01OdQAOjMTwfFlwWMiX3ElArpnQ/lYq8MCVI3UL2mkMNqRPih\n"\
"QANay3rhQ+EFIRPDhypVo+wkDofMYMgKoWRplO8uyOcTzPaq1iKCOXgOeTRdt7/Q\n"\
"KBNCY8cCgYEAwgcQvcjObhK6D39N5w2D+Vtc9sUi+NnjVIkCUhaAPfagAFMiakfL\n"\
"FqhcrARcUo9bK43b/RWOkBPp9B+XZ6mD202lvsCtw2zryYpjxqPynUuqYlalgtV7\n"\
"vaTGVSXRThE5V+hOLh3c9jEyk5AQQURwTGuNF/3atgbdhKFZKF0KsVs=\n"\
"-----END RSA PRIVATE KEY-----"


/**
 * @brief An option to disable Server Name Indication.
 *
 * @note When using a local Mosquitto server setup, SNI needs to be disabled
 * for an MQTT broker that only has an IP address but no hostname. However,
 * SNI should be enabled whenever possible.
 */
#define democonfigDISABLE_SNI    ( pdFALSE )

/**
 * @brief Configuration that indicates if the demo connection is made to the AWS IoT Core MQTT broker.
 *
 * If username/password based authentication is used, the demo will use appropriate TLS ALPN and
 * SNI configurations as required for the Custom Authentication feature of AWS IoT.
 * For more information, refer to the following documentation:
 * https://docs.aws.amazon.com/iot/latest/developerguide/custom-auth.html#custom-auth-mqtt
 *
 * #define democonfigUSE_AWS_IOT_CORE_BROKER    ( 1 )
 */
#define democonfigUSE_AWS_IOT_CORE_BROKER    ( 1 )

/**
 * @brief The username value for authenticating client to the MQTT broker when
 * username/password based client authentication is used.
 *
 * For AWS IoT MQTT broker, refer to the AWS IoT documentation below for
 * details regarding client authentication with a username and password.
 * https://docs.aws.amazon.com/iot/latest/developerguide/custom-authentication.html
 * An authorizer setup needs to be done, as mentioned in the above link, to use
 * username/password based client authentication.
 *
 * #define democonfigCLIENT_USERNAME    "...insert here..."
 */

/**
 * @brief The password value for authenticating client to the MQTT broker when
 * username/password based client authentication is used.
 *
 * For AWS IoT MQTT broker, refer to the AWS IoT documentation below for
 * details regarding client authentication with a username and password.
 * https://docs.aws.amazon.com/iot/latest/developerguide/custom-authentication.html
 * An authorizer setup needs to be done, as mentioned in the above link, to use
 * username/password based client authentication.
 *
 * #define democonfigCLIENT_PASSWORD    "...insert here..."
 */

/**
 * @brief The name of the operating system that the application is running on.
 * The current value is given as an example. Please update for your specific
 * operating system.
 */
#define democonfigOS_NAME                   "FreeRTOS"

/**
 * @brief The version of the operating system that the application is running
 * on. The current value is given as an example. Please update for your specific
 * operating system version.
 */
#define democonfigOS_VERSION                tskKERNEL_VERSION_NUMBER

/**
 * @brief The name of the hardware platform the application is running on. The
 * current value is given as an example. Please update for your specific
 * hardware platform.
 */
#define democonfigHARDWARE_PLATFORM_NAME    "WinSim"

/**
 * @brief The name of the MQTT library used and its version, following an "@"
 * symbol.
 */
#define democonfigMQTT_LIB                  "core-mqtt@1.0.0"

/**
 * @brief Whether to use mutual authentication. If this macro is not set to 1
 * or not defined, then plaintext TCP will be used instead of TLS over TCP.
 */
#define democonfigUSE_TLS                   1

/**
 * @brief Set the stack size of the main demo task.
 *
 * In the Windows port, this stack only holds a structure. The actual
 * stack is created by an operating system thread.
 */
#define democonfigDEMO_STACKSIZE            configMINIMAL_STACK_SIZE

#define mqttexampleNETWORK_BUFFER_SIZE 5000

/**********************************************************************************
* Error checks and derived values only below here - do not edit below here. -----*
**********************************************************************************/


/* Compile time error for some undefined configs, and provide default values
 * for others. */
#ifndef democonfigMQTT_BROKER_ENDPOINT
    #error "Please define democonfigMQTT_BROKER_ENDPOINT in demo_config.h."
#endif

#ifndef democonfigCLIENT_IDENTIFIER

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique so edit as required to ensure no two clients connecting to the
 * same broker use the same client identifier.  Using a #define is for convenience
 * of demonstration only - production devices should use something unique to the
 * device that can be read from software - such as a production serial number.
 */
    #error  "Please define democonfigCLIENT_IDENTIFIER in demo_config.h to something unique for this device."
#endif


#if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 )
    #ifndef democonfigROOT_CA_PEM
        #error "Please define Root CA certificate of the MQTT broker(democonfigROOT_CA_PEM) in demo_config.h."
    #endif

/* If no username is defined, then a client certificate/key is required. */
    #ifndef democonfigCLIENT_USERNAME

/*
 *!!! Please note democonfigCLIENT_PRIVATE_KEY_PEM in used for
 *!!! convenience of demonstration only.  Production devices should
 *!!! store keys securely, such as within a secure element.
 */

        #ifndef democonfigCLIENT_CERTIFICATE_PEM
            #error "Please define client certificate(democonfigCLIENT_CERTIFICATE_PEM) in demo_config.h."
        #endif
        #ifndef democonfigCLIENT_PRIVATE_KEY_PEM
            #error "Please define client private key(democonfigCLIENT_PRIVATE_KEY_PEM) in demo_config.h."
        #endif
    #else

/* If a username is defined, a client password also would need to be defined for
 * client authentication. */
        #ifndef democonfigCLIENT_PASSWORD
            #error "Please define client password(democonfigCLIENT_PASSWORD) in demo_config.h for client authentication based on username/password."
        #endif

/* AWS IoT MQTT broker port needs to be 443 for client authentication based on
 * username/password. */
        #if defined( democonfigUSE_AWS_IOT_CORE_BROKER ) && democonfigMQTT_BROKER_PORT != 443
            #error "Broker port(democonfigMQTT_BROKER_PORT) should be defined as 443 in demo_config.h for client authentication based on username/password in AWS IoT Core."
        #endif
    #endif /* ifndef democonfigCLIENT_USERNAME */

    #ifndef democonfigMQTT_BROKER_PORT
        #define democonfigMQTT_BROKER_PORT    ( 8883 )
    #endif
#else /* if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 ) */
    #ifndef democonfigMQTT_BROKER_PORT
        #define democonfigMQTT_BROKER_PORT    ( 1883 )
    #endif
#endif /* if defined( democonfigUSE_TLS ) && ( democonfigUSE_TLS == 1 ) */

/**
 * @brief ALPN (Application-Layer Protocol Negotiation) protocol name for AWS IoT MQTT.
 *
 * This will be used if democonfigMQTT_BROKER_PORT is configured as 443 for the AWS IoT MQTT broker.
 * Please see more details about the ALPN protocol for AWS IoT MQTT endpoint
 * in the link below.
 * https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/
 */
#define AWS_IOT_MQTT_ALPN           "\x0ex-amzn-mqtt-ca"

/**
 * @brief This is the ALPN (Application-Layer Protocol Negotiation) string
 * required by AWS IoT for password-based authentication using TCP port 443.
 */
#define AWS_IOT_CUSTOM_AUTH_ALPN    "\x04mqtt"

/**
 * Provide default values for undefined configuration settings.
 */
#ifndef democonfigOS_NAME
    #define democonfigOS_NAME    "FreeRTOS"
#endif

#ifndef democonfigOS_VERSION
    #define democonfigOS_VERSION    tskKERNEL_VERSION_NUMBER
#endif

#ifndef democonfigHARDWARE_PLATFORM_NAME
    #define democonfigHARDWARE_PLATFORM_NAME    "WinSim"
#endif

#ifndef democonfigMQTT_LIB
    #define democonfigMQTT_LIB    "core-mqtt@1.0.0"
#endif

/**
 * @brief The MQTT metrics string expected by AWS IoT.
 */
#define AWS_IOT_METRICS_STRING                                 \
    "?SDK=" democonfigOS_NAME "&Version=" democonfigOS_VERSION \
    "&Platform=" democonfigHARDWARE_PLATFORM_NAME "&MQTTLib=" democonfigMQTT_LIB

/**
 * @brief The length of the MQTT metrics string expected by AWS IoT.
 */
#define AWS_IOT_METRICS_STRING_LENGTH    ( ( uint16_t ) ( sizeof( AWS_IOT_METRICS_STRING ) - 1 ) )

#ifdef democonfigCLIENT_USERNAME

/**
 * @brief Append the username with the metrics string if #democonfigCLIENT_USERNAME is defined.
 *
 * This is to support both metrics reporting and username/password based client
 * authentication by AWS IoT.
 */
    #define CLIENT_USERNAME_WITH_METRICS    democonfigCLIENT_USERNAME AWS_IOT_METRICS_STRING
#endif

/**
 * @brief Length of client identifier.
 */
#define democonfigCLIENT_IDENTIFIER_LENGTH    ( ( uint16_t ) ( sizeof( democonfigCLIENT_IDENTIFIER ) - 1 ) )

/**
 * @brief Length of MQTT server host name.
 */
#define democonfigBROKER_ENDPOINT_LENGTH      ( ( uint16_t ) ( sizeof( democonfigMQTT_BROKER_ENDPOINT ) - 1 ) )


#endif /* DEMO_CONFIG_H */
