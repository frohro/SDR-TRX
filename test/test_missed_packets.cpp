#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "test_missed_packets.h"

// WiFi settings
const char *ssid = "Frohne-2.4GHz";
const char *password = "";

// UDP settings
IPAddress remoteIP(192, 168, 1, 101);
unsigned int remotePort = 12345;
WiFiUDP Udp;

// Packet settings
const int bufferSize = 1458;
int packetNumber = 100;
const int packetNumberSize = sizeof(uint32_t);

void setup()
{
    Serial.begin();
    // Initialize WiFi
    delay(5000);  // Give time for serial monitor to show up.
    WiFi.begin(ssid);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Initialize UDP
    Udp.begin(12345);
    
    // Create a buffer for the packet
    uint8_t buffer[bufferSize];

    for (int i = 0; i < packetNumber; i++)
    {

        // Fill the buffer with zeros
        memset(buffer, 0x00, bufferSize);

        // Set the packet number in the buffer
        memcpy(buffer, &i, packetNumberSize);

        // Send the packet
        if (!Udp.beginPacket(remoteIP, remotePort))
        {
            Serial.println("Failed to begin packet");
            return;
        }
        if (!Udp.beginPacket(remoteIP, remotePort))
        {
            Serial.println("Failed to begin packet");
            return;
        }
        if (!Udp.write(buffer, bufferSize))
        {
            Serial.println("Failed to write packet");
            return;
        }
        if (!Udp.endPacket())
        {
            Serial.println("Failed to end packet");
            return;
        }

        // Wait for a short period before sending the next packet
        delay(2);
    }
    Serial.println("Done sending packets");
    delay(5000);
    Serial.printf("packetNumber: %d\n", packetNumber);  // This should be 0
}

void loop()
{
}