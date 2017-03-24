// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <Adafruit_WINC1500.h>
#include <Adafruit_WINC1500Client.h>
#include <Adafruit_WINC1500Server.h>
#include <Adafruit_WINC1500SSLClient.h>
#include <Adafruit_WINC1500Udp.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <Adafruit_BME280.h>
#include <time.h>
#include <sys/time.h>
#include <AzureIoTHub.h>
#include <AzureIoTUtility.h>
#include <AzureIoTProtocol_HTTP.h>
#include <Wire.h>
#include <SPI.h>

#include "NTPClient.h"

#include "config.h"

static bool messagePending = false;

static char *connectionString;
static char *ssid;
static char *pass;

void blinkLED()
{
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
}

// Setup the WINC1500 connection with the pins above and the default hardware SPI.
Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);

void initWifi()
{
    // Attempt to connect to Wifi network:
    LogInfo("Attempting to connect to SSID: %s", ssid);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED)
    {
        // Get Mac Address and show it.
        // WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The M0 WiFi board should
        // start from mac[5] to mac[0], but some other kinds of board run in the oppsite direction.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        LogInfo("You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.",
                mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], ssid);
        WiFi.begin(ssid, pass);
        delay(10000);
    }

    LogInfo("Connected to wifi %s", ssid);
}

void initTime()
{
    Adafruit_WINC1500UDP _udp;

    time_t epochTime = (time_t)-1;

    NTPClient ntpClient;
    ntpClient.begin();

    while (true)
    {
        epochTime = ntpClient.getEpochTime("pool.ntp.org");

        if (epochTime == (time_t)-1)
        {
            LogInfo("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            LogInfo("Fetched NTP epoch time is: %lu", epochTime);
            break;
        }
    }

    ntpClient.end();

    struct timeval tv;
    tv.tv_sec = epochTime;
    tv.tv_usec = 0;

    settimeofday(&tv, NULL);
}

static Adafruit_WINC1500SSLClient sslClient; // for Adafruit WINC1500

/*
 * The new version of AzureIoTHub library change the AzureIoTHubClient signature.
 * As a temporary solution, we will test the definition of AzureIoTHubVersion, which is only defined
 *    in the new AzureIoTHub library version. Once we totally deprecate the last version, we can take
 *    the #ifdef out.
 */
#ifdef AzureIoTHubVersion
static AzureIoTHubClient iotHubClient;
static void beginIoThubClient()
{
    iotHubClient.begin(sslClient);
}
#else
static AzureIoTHubClient iotHubClient(sslClient);
static void beginIoThubClient()
{
    iotHubClient.begin();
}
#endif

static void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    if (IOTHUB_CLIENT_CONFIRMATION_OK == result)
    {
        LogInfo("Message sent to Azure IoT Hub");
        blinkLED();
    }
    else
    {
        LogInfo("Failed to send message to Azure IoT Hub");
    }
    messagePending = false;
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, char *buffer, bool temperatureAlert)
{
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char *)buffer, strlen(buffer));
    if (messageHandle == NULL)
    {
        LogInfo("unable to create a new IoTHubMessage");
    }
    else
    {
        MAP_HANDLE properties = IoTHubMessage_Properties(messageHandle);
        Map_Add(properties, "temperatureAlert", temperatureAlert ? "true" : "false");
        LogInfo("Sending message: %s", buffer);
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, NULL) != IOTHUB_CLIENT_OK)
        {
            LogInfo("Failed to hand over the message to IoTHubClient");
        }
        else
        {
            messagePending = true;
            LogInfo("IoTHubClient accepted the message for delivery");
        }

        IoTHubMessage_Destroy(messageHandle);
    }
}

IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char *buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        LogInfo("unable to IoTHubMessage_GetByteArray\r\n");
        result = IOTHUBMESSAGE_REJECTED;
    }
    else
    {
        /*buffer is not zero terminated*/
        char temp[size + 1];
        memcpy(temp, buffer, size);
        temp[size] = '\0';
        LogInfo("Receiving message: %s", temp);
        result = IOTHUBMESSAGE_ACCEPTED;
    }

    return result;
}

#if SIMULATED_DATA

void initSensor()
{
    // use SIMULATED_DATA, no sensor need to be inited
}

float readTemperature()
{
    return random(20, 30);
}

float readHumidity()
{
    return random(30, 40);
}

#else

Adafruit_BME280 bme(BME_CS);
void initSensor()
{
    if (!bme.begin())
    {
        LogInfo("Could not find a valid BME280 sensor");
    }
}

float readTemperature()
{
    return bme.readTemperature();
}

float readHumidity()
{
    return bme.readHumidity();
}

#endif

bool readMessage(int messageId, char *payload)
{
    float temperature = readTemperature();
    float humidity = readHumidity();
    StaticJsonBuffer<MESSAGE_MAX_LEN> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["deviceId"] = DEVICE_ID;
    root["messageId"] = messageId;
    bool result = false;

    // NAN is not the valid json, change it to NULL
    if (std::isnan(temperature))
    {
        root["temperature"] = NULL;
    }
    else
    {
        root["temperature"] = temperature;
        if(temperature > TEMPERATURE_ALERT)
        {
            result = true;
        }
    }

    if (std::isnan(humidity))
    {
        root["humidity"] = NULL;
    }
    else
    {
        root["humidity"] = humidity;
    }
    root.printTo(payload, MESSAGE_MAX_LEN);
    return result;
}

void initSerial()
{
    // Start serial and initialize stdout
    Serial.begin(115200);
    while (!Serial)
        ;
    LogInfo("Serial successfully inited");
}

// Read parameters from Serial
void readCredentials()
{
    // malloc for parameters
    ssid = (char *)malloc(SSID_LEN);
    pass = (char *)malloc(PASS_LEN);
    connectionString = (char *)malloc(CONNECTION_STRING_LEN);

    // read from Serial and save to EEPROM
    readFromSerial("Input your Wi-Fi SSID: ", ssid, SSID_LEN, 0);
    readFromSerial("Input your Wi-Fi password: ", pass, PASS_LEN, 0);
    readFromSerial("Input your Azure IoT hub device connection string: ", connectionString, CONNECTION_STRING_LEN, 0);
}

/* Read a string whose length should in (0, lengthLimit) from Serial and save it into buf.
 *
 *        prompt   - the interact message and buf should be allocaled already and return true.
 *        buf      - a part of memory which is already allocated to save string read from serial
 *        maxLen   - the buf's len, which should be upper bound of the read-in string's length, this should > 0
 *        timeout  - If after timeout(ms), return false with nothing saved to buf.
 *                   If no timeout <= 0, this function will not return until there is something read.
 */
bool readFromSerial(char *prompt, char *buf, int maxLen, int timeout)
{
    int timer = 0, delayTime = 1000;
    String input = "";
    if (maxLen <= 0)
    {
        // nothing can be read
        return false;
    }

    LogInfo("%s", prompt);
    while (1)
    {
        input = Serial.readString();
        int len = input.length();
        if (len > maxLen)
        {
            LogInfo("Your input should less than %d character(s), now you input %d characters\n", maxLen, len);
        }
        else if (len > 0)
        {
            // save the input into the buf
            sprintf(buf, "%s", input.c_str());
            return true;
        }

        // if timeout, return false directly
        timer += delayTime;
        if (timeout > 0 && timer >= timeout)
        {
            LogInfo("You input nothing, skip...");
            return false;
        }
        // delay a time before next read
        delay(delayTime);
    }
}

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
void setup()
{
    // enable red LED GPIO for writing
    pinMode(LED_PIN, OUTPUT);
    initSerial();

    delay(2000);

#ifdef WINC_EN
    pinMode(WINC_EN, OUTPUT);
    digitalWrite(WINC_EN, HIGH);
#endif

    readCredentials();

    initWifi();
    initTime();

    initSensor();
    // setup iot hub client which will diliver your message
    beginIoThubClient();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    iotHubClient.setEpochTime(tv.tv_sec);

    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);
    if (iotHubClientHandle == NULL)
    {
        LogInfo("Failed on IoTHubClient_CreateFromConnectionString");
        while (1)
            ;
    }

    // Because it can poll "after 2 seconds" polls will happen
    // effectively at ~3 seconds.
    // Note that for scalabilty, the default value of minimumPollingTime
    // is 25 minutes. For more information, see:
    // https://azure.microsoft.com/documentation/articles/iot-hub-devguide/#messaging
    int minimumPollingTime = 2;
    if (IoTHubClient_LL_SetOption(iotHubClientHandle, "MinimumPollingTime", &minimumPollingTime) != IOTHUB_CLIENT_OK)
    {
        LogInfo("failure to set option \"MinimumPollingTime\"\r\n");
    }

    IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
}

int messageCount = 1;
void loop()
{
    if (!messagePending)
    {
        char messagePayload[MESSAGE_MAX_LEN];
        bool temperatureAlert = readMessage(messageCount, messagePayload);
        sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
        messageCount++;
        delay(INTERVAL);
    }
    IoTHubClient_LL_DoWork(iotHubClientHandle);
}
