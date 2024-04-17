#include <Arduino.h>

const int SW_PIN = 16;
const int SW1_PIN = 18;

void setup() {
    pinMode(SW_PIN, OUTPUT);
}
void setup1()
{
    pinMode(SW1_PIN, OUTPUT);
}

void loop() {
    rp2040.idleOtherCore();
    digitalWrite(SW_PIN, HIGH);
    digitalWrite(SW_PIN, LOW);
    rp2040.resumeOtherCore();
}

void loop1()
{
    digitalWrite(SW1_PIN, HIGH);
    digitalWrite(SW1_PIN, LOW);
}