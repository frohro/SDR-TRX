#include <Arduino.h>

const int SW_PIN = 16;
const int SW1_PIN = 18;

void setup() {
    rp2040.idleOtherCore();
    pinMode(SW_PIN, OUTPUT);
    pinMode(SW1_PIN, OUTPUT);
    delay(100);
    rp2040.resumeOtherCore();
}
void setup1()
{

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