/*
 * This Pico W program will connect to the PCM1808 via I2S and read the data it RATE samples per second.
 * It then sends the dato over WIFI using UDP.  To receive it and plot the results, you can use the 
 * python scripts and this sequence in Linux:
 * $ rm sound_data.csv 
 * $ nc -u -l 12345 > sound_data.csv
 * ^C
 * $ python3 plot_sound_data.py
 * The ^C is to stop the nc process which is receiving the data and saving it to sound_data.csv.
 * You can see that every oonce in a while there will be what appears to be a zero in the data.
 * We will have to remove them.  We also have to remove about 90 samples every second at 96 kHz
 * sampling rate.  (Less at slower sampling rates, because the Pico does not have exactly the right
 * crystal frequency to get exactly the audio rate we need.  We could reduce the load on the network
 * by sending the data in a binary way.  It appears we are getting almost 20 Mbits/s through the WIFI!
 * 
 * Author: Rob Frohne, KL7NA, with help from Perplexity and Github Copilot.
 * 3/9/2024
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

#ifndef STASSID
#define STASSID Frohne-2.4GHz
#endif

WiFiUDP udp;  // For some reason or other, if compiled with platformio on VSCode
// it will not connect to WIFI, but it works with the Arduino IDE.

void setup() {

  WiFi.begin("Frohne-2.4GHz");

   while (WiFi.status() != WL_CONNECTED) {
    delay(10000);
    Serial.println ("Trying to connect to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(12345);
}

const size_t BUFFER_SIZE = 1404;  // Assuming the MTU is 1500 bytes...
char buffer[BUFFER_SIZE];
const char* udpAddress = "192.168.1.101";  // Put your laptop IP here.
const unsigned int udpPort = 12345;


void loop() {
  // Send the buffer via UDP
  udp.beginPacket(udpAddress, udpPort);
  udp.write((const uint8_t*)buffer, BUFFER_SIZE);
  udp.endPacket();
}
