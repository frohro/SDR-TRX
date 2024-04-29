#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

// WiFi settings
const char* ssid = "Frohne-2.4GHz";
const char* password = "";

// UDP settings
IPAddress remoteIP(192, 168, 1, 101);
unsigned int remotePort = 12345;
WiFiUDP Udp;

// Packet settings
const int bufferSize = 1468;
const int packetNumberSize = sizeof(uint32_t);

// Packet number
uint32_t packetNumber = 0;

void setup() {
    // Initialize WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Initialize UDP
    Udp.begin(12345);
}

void loop() {
    // Create a buffer for the packet
    uint8_t buffer[bufferSize];

    // Fill the buffer with zeros
    memset(buffer, 0x00, bufferSize);

    // Set the packet number in the buffer
    memcpy(buffer, &packetNumber, packetNumberSize);

    // Send the packet
    Udp.beginPacket(remoteIP, remotePort);
    Udp.write(buffer, bufferSize);
    Udp.endPacket();

    // Increment the packet number
    packetNumber++;

    // Wait for a short period before sending the next packet
    delay(10);
}