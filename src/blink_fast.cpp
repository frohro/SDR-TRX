#include <Arduino.h>

const int SW_PIN = 16;
const int SW1_PIN = 17;

void setup() {
    pinMode(SW_PIN, OUTPUT);
}
void setup1()
{
    pinMode(SW1_PIN, OUTPUT);
}

void loop() {
    digitalWrite(SW_PIN, HIGH);
    digitalWrite(SW_PIN, LOW);
}

void loop1()
{
    digitalWrite(SW1_PIN, HIGH);
    digitalWrite(SW1_PIN, LOW);
}