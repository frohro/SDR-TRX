#include <Arduino.h>

const int LED_PIN = 16;

void setup() {
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
}