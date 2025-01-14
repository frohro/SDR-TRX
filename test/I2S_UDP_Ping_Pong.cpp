/*
 * This Pico W program will connect to the PCM1808 via I2S and read the data it RATE samples per second.
 * It then sends the dato over WIFI using UDP.  As of 4/11/2024, this is too slow.
 * It is taking 0.7ms for sending one UDP packet.  Something is wrong majorly.  I did not
 * finish debugging it as I'm pretty sure I need multicore.
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

#define RATE 16000
#define MCLK_MULT 256 // 384 for 48 BCK per frame,  256 for 64 BCK per frame

I2S i2s(INPUT);

WiFiUDP udp;
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpPort = 12345;

const uint BUFFER_SIZE = (2*sizeof(int32_t)*180) + sizeof(uint32_t); 
// room for an uint32_t to tell the sequence number of the packet. 
char bufferA[BUFFER_SIZE];
char bufferB[BUFFER_SIZE];
char *currentBuffer = bufferA;
char *sendBuffer = bufferB;
int bufferIndex = 4;
volatile bool dataReady = false;
uint32_t start_time = micros();


void i2sDataReceived()
{
    // Serial.println("Micros");
  
    static int32_t r, l, packet_number = 0;  // Static for a tiny boost in speed.
    int n = i2s.available();
    Serial.println();
    i2s.read32(&l, &r); // Read the next l and r values
    l = l << 9;
    r = r << 9;
    // Fill the current buffer with data
    // Copy the binary data of l and r to the current buffer
    // memcpy(currentBuffer + bufferIndex, &l, sizeof(int32_t));
    bufferIndex += sizeof(int32_t);
    // memcpy(currentBuffer + bufferIndex, &r, sizeof(int32_t));
    bufferIndex += sizeof(int32_t);  // Separate increments could be sped up by using a single increment
    // and pre-computing the number to increment by.  (Maybe the compiler optimizes this already.)
    // Sending ASCII like this is a problem, because once in a while the number may not require
    // the same number of characters.  This will cause a problem.  We need to send the data in binary.

    // If the buffer is full, swap the buffers and add the packet number
    if (bufferIndex == BUFFER_SIZE)
    {   // Send the packet number, swap the buffers, and set the flag.
        // Serial.print("bufferIndex: ");
        // Serial.println(bufferIndex);
        // Serial.print("BUFFER_SIZE: ");
        // Serial.println(BUFFER_SIZE);
        memcpy(currentBuffer, &packet_number, sizeof(int32_t));// Make the first 4 bytes the packet number. 
        char *temp = currentBuffer;  // Swap the buffers
        currentBuffer = sendBuffer;
        sendBuffer = temp;
        bufferIndex = 4;   // Reset the buffer index
        packet_number++;   // Increment the packet number
        dataReady = true;  // Set the flag to indicate data is ready to be sent
        //   Serial.println(micros());
        //   Serial.print(" Delta time: ");
    }
}   

void setup()
{
    Serial.begin();

    i2s.setDATA(2); // These are the pins for the data on the SDR-TRX
    i2s.setBCLK(0);
    i2s.setMCLK(3);
    // Note: LRCK pin is BCK pin plus 1 (1 in this case).
    i2s.setMCLKmult(MCLK_MULT);
    i2s.setSysClk(RATE);
    i2s.setBitsPerSample(24);
    i2s.setFrequency(RATE);
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
    {  // This is getting interrupted at 1/f_s with the I2S callback.  The problem is that the
        // end of a packet can happen in the middle of this code.  This is a problem.  We need to
        udp.beginPacket(udpAddress, udpPort);
        udp.write((const uint8_t *)sendBuffer, BUFFER_SIZE);
        udp.endPacket();
        dataReady = false;
    }

    // Your other main loop code goes here...
}