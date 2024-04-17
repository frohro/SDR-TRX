#include <Arduino.h>
#include <si5351.h>

const int LED_PIN = 19;

void setup() {
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_PIN, HIGH);

    digitalWrite(LED_PIN, LOW);

}