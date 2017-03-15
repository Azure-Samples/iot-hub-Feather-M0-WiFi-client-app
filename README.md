# IoT Hub Feather M0 WiFi Client application
[![Build Status](https://travis-ci.com/Azure-Samples/iot-hub-Feather-M0-WiFi-client-app.svg?token=5ZpmkzKtuWLEXMPjmJ6P&branch=master)](https://travis-ci.com/Azure-Samples/iot-hub-Feather-M0-WiFi-client-app) 

> This repo contains the source code to help you get familiar with Azure IoT using the Azure IoT Adafruit Feather M0 WiFi Starter Kit. You will find the [lesson-based tutorials on Azure.com](#).

This repo contains an arduino application that runs on board Feather M0 WiFi with a BME280 temperature&humidity sensor, and then sends these data to your IoT hub. At the same time, this application receives Cloud-to-Device message from your IoT hub, and takes actions according to the C2D command. 

## Install board with your Arduino IDE
Follow [this page](https://learn.adafruit.com/adafruit-feather-m0-wifi-atwinc1500/using-with-arduino-ide) to install board manager for your M0 WiFi.

## Install libraries
Install the following libraries from `Sketch -> Include library -> Manage libraries`

* Adafruit_WINC1500
* RTCZero
* NTPClient
* AzureIoTHub
* AzureIoTUtility
* AzureIoTProtocol_HTTP
* ArduinoJson
* Adafruit BME280 Library
* Adafruit Unified Sensor

## Connect your sensor with your board
### Connect with a physical BME280 sensor
You can follow the image to connect your BME280 with your Feather M0 WiFi.

![BME280](#)

### DON'T HAVE A PHYSICAL BME280?
You can use the application to simulate temperature&humidity data and send to your IoT hub.
1. Open the `app/config.h` file.
2. Change the `SIMULATED_DATA` value from `false` to `true`.

## Configure and run sample application
Upload the `app.ino` to your board.

### Input your credential information
After you successfully upload the code to your board. You will see some prompt, input your credential information according to the prompts.

### Send Cloud-to-Device command
You can send a C2D message to your device. You can see the device prints out the message and blinks once receiving the message.
