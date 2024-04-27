/* This is to check and see that micros() behaves like I expect it to
    so I can write debugging code to determine how to improve the missed
    packet situation.
    Rob Frohne with the help of Github Copilot.
*/

#include <Arduino.h>
#include <si5351.h>

void setup() {
    // Initialize LED pins as outputs
    Serial.begin(115200);
    Serial.printf("Hello from Core0.  Time is: %d\n", micros());
}

void setup1() {
    Serial.printf("Hello from Core1.  Time is: %d\n", micros());
}

void loop() {
    Serial.printf("Core0 time is: %d\n", micros());
    delay(1000);
}

void loop1() {
    Serial.printf("Core1 time is: %d\n", micros());
    delay(1000);
}