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
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>

#ifndef STASSID
#define STASSID Frohne - 2.4GHz
#endif

#define RATE 96000
#define MCLK_MULT 256 // 384 for 48 BCK per frame,  256 for 64 BCK per frame

I2S i2s(INPUT);

WiFiUDP udp;

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

    udp.begin(12345);
    i2s.begin();
}

const size_t BUFFER_SIZE = 1404; // Assuming the MTU is 1500 bytes...
char buffer[BUFFER_SIZE];
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpPort = 12345;

void loop()
{
    static int32_t r, l;
    size_t bufferIndex = 0;

    // Fill the buffer with 108 lines of "%d\r\n"

    while (bufferIndex < BUFFER_SIZE - 26)
    {     
                         // 13 is the max size of "%d\r\n" with 10 digits
        i2s.read32(&l, &r); // Read the next l and r values
        l = l << 9;
        r = r << 9;
        // This next line is for transferring data over the UART in case you don't have a Pico W,
        //    Serial.printf("%d,%d\r\n", l, r);  // This only works up 16 kHz.
        // With the extra four spaces it should wark at 96 kHz with 16 bit samples
        // and leaving about 90 frames for control packets if we can figure out how
        // to do that.
        unsigned long start_time = micros(); 
        // These next lines create the buffer of slightly less that the MTU (1500 bytes)
        int n = snprintf(buffer + bufferIndex, BUFFER_SIZE - bufferIndex, "%d,%d\r\n", l, r);
        if (n > 0)
        {
            bufferIndex += n;
        }
        else
        {
            Serial.println("Problem with buffer!");
            break;
        }
        unsigned long end_time = micros();
        Serial.printf("Time: %d us\n", end_time - start_time);
    }
    
    // Send the buffer via UDP
    udp.beginPacket(udpAddress, udpPort);
    udp.write((const uint8_t *)buffer, bufferIndex);
    udp.endPacket();

    // Clear the buffer for the next round
    memset(buffer, 0, BUFFER_SIZE);

}