# 1 "/tmp/tmpey_otuj_"
#include <Arduino.h>
# 1 "/home/frohro/Documents/PlatformIO/Projects/SDR-TRX/src/Basic_PCM1808_Testing_UDP_Data_Stream.ino"
# 19 "/home/frohro/Documents/PlatformIO/Projects/SDR-TRX/src/Basic_PCM1808_Testing_UDP_Data_Stream.ino"
#include <Arduino.h>
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

#ifndef STASSID
#define STASSID Frohne-2.4GHz
#endif

#define RATE 16000
#define MCLK_MULT 256

I2S i2s(INPUT);

WiFiUDP udp;
void setup();
void loop();
#line 37 "/home/frohro/Documents/PlatformIO/Projects/SDR-TRX/src/Basic_PCM1808_Testing_UDP_Data_Stream.ino"
void setup() {
  Serial.begin();

  i2s.setDATA(2);
  i2s.setBCLK(0);
  i2s.setMCLK(3);

  i2s.setBitsPerSample(24);
  i2s.setFrequency(RATE);
  i2s.setMCLKmult(MCLK_MULT);
  i2s.setSysClk(RATE);
  i2s.setBuffers(32, 0, 0);

  WiFi.begin("Frohne-2.4GHz");

   while (WiFi.status() != WL_CONNECTED) {
    delay(10000);
    Serial.println ("Trying to connect to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(12345);
  i2s.begin();
}

const size_t BUFFER_SIZE = 1404;
char buffer[BUFFER_SIZE];
const char* udpAddress = "192.168.1.101";
const unsigned int udpPort = 12345;


void loop() {
  static int32_t r, l;
  size_t bufferIndex = 0;


  while (bufferIndex < BUFFER_SIZE - 26) {

    l = l<<9;
    r = r<<9;







    int n = snprintf(buffer + bufferIndex, BUFFER_SIZE - bufferIndex, "%d,%d\r\n", l, r);
    if (n > 0) {
      bufferIndex += n;
    } else {
      Serial.println("Problem with buffer!");
      break;
    }
  }


  udp.beginPacket(udpAddress, udpPort);
  udp.write((const uint8_t*)buffer, bufferIndex);
  udp.endPacket();


  memset(buffer, 0, BUFFER_SIZE);
}