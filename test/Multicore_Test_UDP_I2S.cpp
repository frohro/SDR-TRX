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
#include "pico/mutex.h"

const char *STASSID = "Frohne-2.4GHz";

const int RATE = 48000; // Works with 32 bits per sample at 96 ks/s.  This is 768 kBps.  We cound save by using 24 bits per sample.
const int MCLK_MULT = 256; // 384 for 48 BCK per frame,  256 for 64 BCK per frame

static mutex_t my_mutex;
uint32_t mutex_save;

I2S i2s(INPUT);

WiFiUDP udpData;
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpDataPort = 12345;

#define BUFFER_SIZE 1468 // 183 samples of 8 bytes + 4 bytes for the packet number
#define QUEUE_SIZE 9  // Maximum latency is 8 packets, which is 15 ms.  Average is 2 packets, or 4 ms.  

class CircularBufferQueue
{
private:
    char buffers[QUEUE_SIZE][BUFFER_SIZE];
    uint32_t fillIndex;
    uint32_t emptyIndex;

public:
    CircularBufferQueue() : fillIndex(1), emptyIndex(0) {}

    char *getNextBufferAndUpdate(bool isFiller)
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
        if ((currentIndex + 1) % QUEUE_SIZE != otherIndex) // If we can move to the next buffer...
        {
            currentIndex = (currentIndex + 1) % QUEUE_SIZE;
            rp2040.fifo.push(currentIndex);
            return buffers[currentIndex];
        }
        else
        {
            return nullptr; // Return null if the buffer is full/empty
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
        while (!mutex_try_enter(&my_mutex, &mutex_save))
        {
            // Mutex is locked, so wait here.
        }
        char *buffer = queue.getNextBufferAndUpdate(true);
        mutex_exit(&my_mutex);
        if (buffer != nullptr)
        {
            static int32_t r, l, packet_number = 0;
            uint32_t bufferIndex = 4;
            while (bufferIndex < BUFFER_SIZE - 4) // Leave space for the packet number
            {
                i2s.read32(&l, &r);
                l = l << 8;
                r = r << 8;
                memcpy(buffer + bufferIndex, &l, sizeof(int32_t));  // We could speed this up for the network by sending 24 bits instead of 32.
                bufferIndex += sizeof(int32_t);
                memcpy(buffer + bufferIndex, &r, sizeof(int32_t));
                bufferIndex += sizeof(int32_t);
            }
            memcpy(buffer, &packet_number, sizeof(int32_t));
            packet_number++;
            // Serial.printf("Filled %d\n", packet_number);
        }
    }
};

class BufferEmptyer
{
private:
    CircularBufferQueue &queue;
    char temp_buffer[BUFFER_SIZE];
public:
BufferEmptyer(CircularBufferQueue &q) : queue(q) {
    memset(temp_buffer, 0xff, BUFFER_SIZE);
}

    void emptyBuffer()
    {
        while (!mutex_try_enter(&my_mutex, &mutex_save))
        {
            // Mutex is locked, so wait here.
        }
        char *buffer = queue.getNextBufferAndUpdate(false);
        mutex_exit(&my_mutex);
        if (buffer != nullptr)
        {
            udpData.beginPacket(udpAddress, udpDataPort);  // Needed
            memcpy(temp_buffer, buffer, BUFFER_SIZE);  // If we don't do this, it hangs in the udpData.write below.
            udpData.write((const uint8_t *)&temp_buffer, BUFFER_SIZE); 
            udpData.endPacket();
            // Serial.printf("Sent %d\n", *(int32_t *)buffer); 
        }
    }
};

CircularBufferQueue bufferQueue;

void setup()
{ // This runs on Core0.  It is the UDP setup.
    mutex_init(&my_mutex);
    if (mutex_try_enter(&my_mutex, &mutex_save))  // Synchronze cores so they start about the same time.
    {
        Serial.begin();
        // delay(10);
        WiFi.begin(STASSID);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(1000);
        }
        udpData.begin(udpDataPort);
        // Serial.printf("Connected to %s\n", STASSID);
        mutex_exit(&my_mutex);
    }
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
    mutex_enter_blocking(&my_mutex); // This should syschornize the cores, so one doesn't fill the buffer
    // before the other is ready.
    mutex_exit(&my_mutex);
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