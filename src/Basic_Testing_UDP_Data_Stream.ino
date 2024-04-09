/*
 * Author: Rob Frohne, KL7NA, with help from Perplexity and Github Copilot.
 * 4/9/2024
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

#ifndef STASSID
#define STASSID Frohne - 2.4GHz
#endif

WiFiUDP udp;
void setup()
{

    WiFi.begin("Frohne-2.4GHz");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10000);
    }

    udp.begin(12345);
}

const size_t BUFFER_SIZE = 1404; // Assuming the MTU is 1500 bytes...
char buffer[BUFFER_SIZE];
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpPort = 12345;

void loop()
{
    // Send the buffer via UDP
    udp.beginPacket(udpAddress, udpPort);
    udp.write((const uint8_t *)buffer, BUFFER_SIZE);
    udp.endPacket();
}
