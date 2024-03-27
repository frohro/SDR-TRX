#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid     = "Frohne-2.4GHz";
const char* password = "your_PASSWORD";

WiFiUDP Udp;
unsigned int localUdpPort = 12345;  // local port to listen for UDP packets
IPAddress serverIp(192, 168, 1, 101); 

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}

void loop()
{
  // Send a UDP packet
  Udp.beginPacket(serverIp, 12345);
  Udp.write("Hello, world!");
  Udp.endPacket();
}