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
 * Author: Rob Frohne, KL7NA, with help from Github Copilot.
 * 4/9/2024
 */
#include <Arduino.h>
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

#ifndef STASSID
#define STASSID "Frohne-2.4GHz"
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
{
private:
    char buffers[QUEUE_SIZE][BUFFER_SIZE];
    uint32_t fillIndex;
    uint32_t emptyIndex;

public:
    CircularBufferQueue() : fillIndex(1), emptyIndex(0) {}

    // Get a pointer to the next buffer to be processed
    char *getNextBuffer(bool isFiller)
    {
        uint32_t currentIndex = isFiller ? fillIndex : emptyIndex;
        uint32_t otherIndex, temp;
        if (rp2040.fifo.available() == 0)
        {
            otherIndex = isFiller ? emptyIndex : fillIndex; // Return nullptr if the other op is not done.
        }
        else
        {
            while (rp2040.fifo.pop_nb(&temp)) // Gets most recent otherIndex, empties FIFO
            {
                otherIndex = temp;
            }
        }

        // Check if the current index is about to overtake the other index
        // Serial.printf("Current index: %d, Other index: %d\n", currentIndex, otherIndex);
        if ((currentIndex + 1) % QUEUE_SIZE != otherIndex)
        {
            return buffers[currentIndex % QUEUE_SIZE];
        }
        else
        {
            return nullptr; // Return null if the buffer is full/empty
        }
    }

    // Move to the next buffer to be processed
    void moveToNextBuffer(bool isFiller)
    {
        uint32_t &currentIndex = isFiller ? fillIndex : emptyIndex;
        uint32_t otherIndex, temp;
        if (rp2040.fifo.available() == 0)
        {
            otherIndex = isFiller ? emptyIndex : fillIndex; // Return nullptr if the other op is not done.
        }
        else
        {
            while (rp2040.fifo.pop_nb(&temp)) // Gets most recent otherIndex, empties FIFO
            {                                 // This does not appear to work if the FIFO is empty.  Need to check if it is empty.
                otherIndex = temp;
            }
        }
        if ((currentIndex + 1) % QUEUE_SIZE != otherIndex)
        {
            currentIndex = (currentIndex + 1) % QUEUE_SIZE;
            rp2040.fifo.push(currentIndex);
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
        rp2040.idleOtherCore();
        char *buffer = queue.getNextBuffer(true);
        rp2040.resumeOtherCore();
        // Serial.printf("Got filler buffer %p\n", buffer);
        if (buffer != nullptr)
        {
            static int32_t r, l, packet_number = 0;
            uint32_t bufferIndex = 4;

            while (bufferIndex < BUFFER_SIZE - 4) // Leave space for the packet number
            {
                i2s.read32(&l, &r);
                l = l << 9;
                r = r << 9;
                memcpy(buffer + bufferIndex, &l, sizeof(int32_t));
                bufferIndex += sizeof(int32_t);
                memcpy(buffer + bufferIndex, &r, sizeof(int32_t));
                bufferIndex += sizeof(int32_t);
            }

            memcpy(buffer, &packet_number, sizeof(int32_t));
            packet_number++;
            rp2040.idleOtherCore();
            queue.moveToNextBuffer(true);
            rp2040.resumeOtherCore();
            Serial.printf("Filled packet %d\n", packet_number);
        }
    }
};

class BufferEmptyer
{
private:
    CircularBufferQueue &queue;

public:
    BufferEmptyer(CircularBufferQueue &q) : queue(q) {}

    void emptyBuffer()
    {
        rp2040.idleOtherCore();
        char *buffer = queue.getNextBuffer(false);
        rp2040.resumeOtherCore();
        Serial.printf("Got emptying buffer %p\n", buffer);
        if (buffer != nullptr)
        {
            udp.beginPacket(udpAddress, udpPort);
            udp.write((const uint8_t *)&buffer, BUFFER_SIZE);
            udp.endPacket();
            rp2040.idleOtherCore();
            queue.moveToNextBuffer(false);
            rp2040.resumeOtherCore();
            Serial.printf("Sent packet %d\n", *(int32_t *)buffer);
        }
    }
};

CircularBufferQueue bufferQueue;

void setup()
{ // This runs on Core0.  It is the UDP setup.
    Serial.begin();
    WiFi.begin(STASSID);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
    }
    udp.begin(udpPort);
    rp2040.restartCore1();  // Connecting to WIFI can take some time.  This synchronizes things (I hope).
    Serial.printf("Connected to %s\n", STASSID);
}

void setup1()
{                   // This runs on Core1.  It is the I2S setup.
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
    Serial.printf("Core 0\n");
    static BufferEmptyer emptyer(bufferQueue);
    emptyer.emptyBuffer(); // Empty the buffer
}

void loop1()
{ // This should run on Core1.  It is the I2S loop.
    Serial.printf("Core 1\n");
    static BufferFiller filler(bufferQueue);
    filler.fillBuffer(); // Fill the buffer
}