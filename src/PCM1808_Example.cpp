#include "PCM1808_I2S_UDP.h"

// Things you might want to change for your situation...
IPAddress remoteIp(192, 168, 1, 101); // Put your laptop IP here.
const unsigned int DATA_UDPPORT = 12345; // UDP server port
const char* STASSID = "Frohne-2.4GHz"; // Put your access point SSID here. 
const int RATE = 96000; // Works with 32 bits per sample at 96 ks/s.  This is 768 kBps.  We cound save by using 24 bits per sample.
const int MCLK_MULT = 256; // 384 for 48 BCK per frame,  256 for 64 BCK per frame
const int BITS_PER_SAMPLE_SENT = 32;

I2S i2s(INPUT);
WiFiUDP udpData; 

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
        udpData.begin(DATA_UDPPORT);
        Serial.printf("Connected to %s\n", STASSID);
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