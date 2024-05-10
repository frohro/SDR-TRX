#include <Arduino.h>
#include <si5351.h>

void setup()
{
    // Initialize the serial port
    Serial.begin();
    // Enable the watchdog timer with a timeout of 2 seconds
    rp2040.wdt_begin(2000);
}

void loop()
{
    // Reset the watchdog timer
    rp2040.wdt_reset();

    Serial.printf("Time: %lu\n", millis());

    // Delay for 1 second
    delay(1000);
}