
#ifndef PCM1808_I2S_UDP_H
#define PCM1808_I2S_UDP_H

#include <Arduino.h>
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>
#include <pico/mutex.h>

extern uint32_t mutex_save;
extern mutex_t my_mutex; // Mutex for thread safety
extern WiFiUDP udpData;
extern IPAddress remoteIP;
extern const char* STASSID;

extern I2S i2s;

const int RATE = 96000; // Works with 32 bits per sample at 96 ks/s.  This is 768 kBps.  We cound save by using 24 bits per sample.
const int MCLK_MULT = 256; // 384 for 48 BCK per frame,  256 for 64 BCK per frame

const unsigned int DATA_UDPPORT = 12345; // UDP server port

#define BUFFER_SIZE 1468 // 183 samples of 8 bytes + 4 bytes for the packet number
#define QUEUE_SIZE 9 // Maximum latency is 8 packets, which is 15 ms.  Average is 2 packets, or 4 ms.

class CircularBufferQueue
{
private:
    char buffers[QUEUE_SIZE][BUFFER_SIZE]; 
    uint32_t fillIndex;
    uint32_t emptyIndex;

public:
    CircularBufferQueue();
    char *getNextBufferAndUpdate(bool isFiller);
};

class BufferFiller
{
private:
    CircularBufferQueue &queue;
    
public:
    BufferFiller(CircularBufferQueue &q);
    void fillBuffer();
};

class BufferEmptyer
{
private:
    CircularBufferQueue &queue;
    char temp_buffer[BUFFER_SIZE];
    
public:
    BufferEmptyer(CircularBufferQueue &q);
    void emptyBuffer();
};

#endif
