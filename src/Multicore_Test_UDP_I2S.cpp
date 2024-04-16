/*
 * Project Name: Multicore_Test_UDP_I2S
 * File Name: Multicore_Test_UDP_I2S.cpp
 * This is a test of the multicore Arudino Pico librory for the Pico W
 * to use both cors and a circular queue of buffers to send data over UDP from the I2S
 * using the PCM1808.
 *
 * This Pico W program will connect to the PCM1808 via I2S and read the data it RATE samples per second.
 * It then sends the dato over WIFI using UDP.
 *
 *
 * Author: Rob Frohne, KL7NA, with help from Perplexity and Github Copilot.
 * 4/9/2024
 */
#include <Arduino.h>
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>
#include <atomic> // Do we need this?

#ifndef STASSID
#define STASSID Frohne - 2.4GHz
#endif

// Use #define for common constants to both cores.
#define RATE 48000 // To do 96000, we need to use 24 bits instead of 32.
// For now we will use 32 bits for debugging ease.
#define MCLK_MULT 256 // 384 for 48 BCK per frame,  256 for 64 BCK per frame

I2S i2s(INPUT);

WiFiUDP udp;
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpPort = 12345;

#define BUFFER_SIZE 1468 // 183 samples of 4 bytes + 4 bytes for the packet number
#define QUEUE_SIZE 9

class CircularBufferQueue
{ // Need to use rp2040.fifo for this.
private:
    char buffers[QUEUE_SIZE][BUFFER_SIZE];
    std::atomic<int> fillIndex;  // Index for filling the buffer
    std::atomic<int> emptyIndex; // Index for emptying the buffer

public:
    // Constructor
    CircularBufferQueue() : fillIndex(0), emptyIndex(0) {}

    // Get a pointer to the next buffer to be filled
    char *getNextFillBuffer()
    {
        // Check if the fill index is about to overtake the empty index
        if ((fillIndex + 1) % QUEUE_SIZE != emptyIndex)
        {
            return buffers[fillIndex % QUEUE_SIZE];
        }
        else
        {
            return nullptr; // Return null if the buffer is full
        }
    }

    // Move to the next buffer to be filled
    void moveToNextFillBuffer()
    {
        if ((fillIndex + 1) % QUEUE_SIZE != emptyIndex)
        {
            fillIndex = (fillIndex + 1) % QUEUE_SIZE;
        }
    }

    // Get a pointer to the next buffer to be emptied
    char *getNextEmptyBuffer()
    {
        // Check if the empty index is about to overtake the fill index
        if (emptyIndex != fillIndex)
        {
            return buffers[emptyIndex % QUEUE_SIZE];
        }
        else
        {
            return nullptr; // Return null if the buffer is empty
        }
    }

    // Move to the next buffer to be emptied
    void moveToNextEmptyBuffer()
    {
        if (emptyIndex != fillIndex)
        {
            emptyIndex = (emptyIndex + 1) % QUEUE_SIZE;
        }
    }
};

class BufferFiller
{
private:
    CircularBufferQueue &queue;

public:
    BufferFiller(CircularBufferQueue &q) : queue(q) {}

    void fillBuffer()
    {
        char *buffer = queue.getNextFillBuffer();
        if (buffer != nullptr)
        {                                                      // There is space to fill the buffer with data
            static int32_t r, l, packet_number = 0;            // Static r, l for a tiny boost in speed.
            uint32_t bufferIndex = 4;                          // Leave four bytes for the packet number.
            i2s.read32(&l, &r);                                // Read the next l and r values (blocking)
            l = l << 9;                                        // These should be 8 I think, but this works and 8 does not.
            r = r << 9;                                        // https://github.com/earlephilhower/arduino-pico/issues/2037
            memcpy(buffer + bufferIndex, &l, sizeof(int32_t)); // These are sending 2 32 bit integers per sample.
            bufferIndex += sizeof(int32_t);
            memcpy(buffer + bufferIndex, &r, sizeof(int32_t));
            bufferIndex += sizeof(int32_t);
            if (bufferIndex == BUFFER_SIZE)
            {
                memcpy(buffer, &packet_number, sizeof(int32_t)); // Before you reinitialize.
                packet_number++;
                queue.moveToNextFillBuffer();
            }
        }
    };
};

class BufferEmptyer
{
private:
    CircularBufferQueue &queue;

public:
    BufferEmptyer(CircularBufferQueue &q) : queue(q) {}

    void emptyBuffer()
    {
        char *buffer = queue.getNextEmptyBuffer();
        if (buffer != nullptr)
        {
            udp.beginPacket(udpAddress, udpPort);
            udp.write((const uint8_t *)&buffer, BUFFER_SIZE);
            udp.endPacket();
            queue.moveToNextEmptyBuffer();
        }
    }
};

CircularBufferQueue bufferQueue;
uint32_t packet_number = 0;

void setup()
{ // This runs on Core0.  It is the UDP setup.
    Serial.begin();
    WiFi.begin("Frohne-2.4GHz");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10000);
    }
    udp.begin(12345);
}

void setup1()
{ // This runs on Core1.  It is the I2S setup.
    i2s.setDATA(2); // These are the pins for the data on the SDR-TRX
    i2s.setBCLK(0);
    i2s.setMCLK(3);
    // Note: LRCK pin is BCK pin plus 1 (1 in this case).
    i2s.setSysClk(RATE);
    i2s.setBitsPerSample(24);
    i2s.setFrequency(RATE);
    i2s.setMCLKmult(MCLK_MULT);
    i2s.setBuffers(32, 0, 0);
    i2s.begin();
}

void loop()
{ // This should run on Core0.  It is the UDP loop.
    static BufferEmptyer emptyer(bufferQueue);
    emptyer.emptyBuffer(); // Empty the buffer
}

void loop1()
{ // This should run on Core1.  It is the I2S loop.
    static BufferFiller filler(bufferQueue);
    filler.fillBuffer(); // Fill the buffer
}