
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
extern IPAddress remoteIp;
extern const char* STASSID;
extern const int BITS_PER_SAMPLE_SENT;

extern I2S i2s;

extern const int RATE; // Works with 32 bits per sample at 96 ks/s.  This is 768 kBps.  We cound save by using 24 bits per sample.
extern const int MCLK_MULT; // 384 32 bit for 48 BCK per frame,  256 for 64 BCK per frame

extern const unsigned int DATA_UDPPORT; // UDP server port

#define BUFFER_SIZE 1468 // 1468 => 183 samples of 8 bytes + 4 bytes for the packet number
#define QUEUE_SIZE 15 // Must be odd.  
// With 9, the maximum latency is 8 packets, which is 15 ms.  Average is 2 packets, or 4 ms.

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
    int DELAY_TIME;
    static char debugBuffer[1024]; // Buffer for debug messages
    static int debugBufferIndex;
    
public:
    BufferEmptyer(CircularBufferQueue &q);
    static void addDebugMessage(const char* format, ...);
    static void printDebugMessages();
    void emptyBuffer();
};

#endif
