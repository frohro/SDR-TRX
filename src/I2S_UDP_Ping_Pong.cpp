/*
 * This Pico W program will connect to the PCM1808 via I2S and read the data it RATE samples per second.
 * It then sends the dato over WIFI using UDP.  
 *
 * Author: Rob Frohne, KL7NA, with help from Perplexity and Github Copilot.
 * 4/9/2024
 */
#include <Arduino.h>
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

#ifndef STASSID
#define STASSID Frohne - 2.4GHz
#endif

#define RATE 96000
#define MCLK_MULT 256 // 384 for 48 BCK per frame,  256 for 64 BCK per frame
#define BUFFER_SIZE 1400

I2S i2s(INPUT);

WiFiUDP udp;
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpPort = 12345;

char bufferA[BUFFER_SIZE];
char bufferB[BUFFER_SIZE];
char *currentBuffer = bufferA;
char *sendBuffer = bufferB;
int bufferIndex = 0;
volatile bool dataReady = false;

void i2sDataReceived()
{
    Serial.println("Data received");
    static int32_t r, l;
    i2s.read32(&l, &r); // Read the next l and r values
    l = l << 9;
    r = r << 9;
    // Fill the current buffer with data
    int n = snprintf(currentBuffer + bufferIndex, BUFFER_SIZE - bufferIndex, "%d,%d\r\n", l, r);
    bufferIndex += n;

    // If the buffer is full, swap the buffers
    if (bufferIndex >= BUFFER_SIZE)
    {
        char *temp = currentBuffer;
        currentBuffer = sendBuffer;
        sendBuffer = temp;
        bufferIndex = 0;

        // Set the flag to indicate data is ready to be sent
        dataReady = true;
    }
}

void setup()
{
    Serial.begin();

    i2s.setDATA(2); // These are the pins for the data on the SDR-TRX
    i2s.setBCLK(0);
    i2s.setMCLK(3);
    // Note: LRCK pin is BCK pin plus 1 (1 in this case).
    i2s.setBitsPerSample(24);
    i2s.setFrequency(RATE);
    i2s.setMCLKmult(MCLK_MULT);
    i2s.setSysClk(RATE);
    i2s.setBuffers(32, 0, 0);

    WiFi.begin("Frohne-2.4GHz");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10000);
        Serial.println("Trying to connect to WiFi...");
    }
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    udp.begin(udpPort);

    // Set the callback function
    i2s.onReceive(i2sDataReceived);
    i2s.begin();
}

void loop()
{
    // If data is ready, send it over UDP
    if (dataReady)
    {
        udp.beginPacket(udpAddress, udpPort);
        udp.write((const uint8_t *)sendBuffer, BUFFER_SIZE);
        udp.endPacket();
        dataReady = false;
    }

    // Your other main loop code goes here...
}