// Copyright (c) Arduino. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <WiFi101.h>
#include <WiFiUdp.h>
#include <stdio.h>
#include <stdint.h>
#ifndef NTPCLIENT_H
#define NTPCLIENT_H

const int NTP_PACKET_SIZE = 48;
const int NTP_PORT = 123;

const int DEFAULT_NTP_TIMEOUT = 10000;

class NTPClient
{
    public:
        NTPClient();
        int begin();
        uint32_t getEpochTime(const char* host, int port = NTP_PORT, unsigned long timeout = DEFAULT_NTP_TIMEOUT);
        void end();

    private:
        void prepareRequest();
        void sendRequest(const char* host, int port);
        int receiveResponse(unsigned long timeout);
        uint32_t parseResponse();

        char        _buffer[NTP_PACKET_SIZE];
        WiFiUDP     _udp;
};

#endif