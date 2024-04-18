/* Basic_Testing_UDP_Data_Stream for speed.  I get 990 kB/s with this code.
 * I am using the Ubuntu System Monitor to measure this.
 * Author: Rob Frohne, KL7NA, with help from Perplexity and Github Copilot.
 * 4/9/2024
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h> // This is needed so I don't have to edit the libraries out of the platform.ini

static mutex_t my_mutex;

WiFiUDP udp;
void setup()
{

    WiFi.begin("Frohne-2.4GHz");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10000);
    }

    udp.begin(12345);
    Serial.begin(115200);
}

const size_t BUFFER_SIZE = 1470; // Assuming the MTU is 1500 bytes...
char buffer[BUFFER_SIZE];
const char *udpAddress = "192.168.1.101"; // Put your laptop IP here.
const unsigned int udpPort = 12345;
uint32_t start_time;
uint32_t total_time = 0;
uint32_t n = 0;

void loop()
{
    // Send the buffer via UDP over and over again.
    // Read the data rate using the Ubuntu System Monitor.
    udp.beginPacket(udpAddress, udpPort); // Takes about 7 us.
    if (rp2040.fifo.available() > 0)
    {
        rp2040.fifo.pop_nb(&junk);
    }
    start_time = micros();

    Serial.println("Mutex entered Core0.");
    udp.write((const uint8_t *)buffer, BUFFER_SIZE); // Takes about 810 us.
    udp.endPacket();                                 // Takes about 670 us
    int time_elapsed = micros() - start_time;
    total_time += time_elapsed;
    Serial.println("Sent packet.");
    if (n++ == 5000)
    {
        Serial.printf("Time per statement: %f\n", (float)total_time / n);
        start_time = micros();
        total_time = 0;
        n = 0;
    }
}
