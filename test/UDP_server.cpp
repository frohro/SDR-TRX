#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";
unsigned int localPort = 12346;

WiFiUDP Udp;

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    Udp.begin(localPort);
    Serial.print("UDP server started on port ");
    Serial.println(localPort);
}

void loop() {
    int packetSize = Udp.parsePacket();
    
    if (packetSize) {
        char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
        int len = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
        
        if (len > 0) {
            packetBuffer[len] = 0;
        }
        
        Serial.print("Received packet of size ");
        Serial.println(len);
        Serial.print("From ");
        IPAddress remoteIp = Udp.remoteIP();
        Serial.print(remoteIp);
        Serial.print(", port ");
        Serial.println(Udp.remotePort());
        Serial.print("Message: ");
        Serial.println(packetBuffer);
        
        // Process the received packet here
        
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write("ACK");
        Udp.endPacket();
    }
}