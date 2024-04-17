#include <Arduino.h>
#include <si5351.h>

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
    rp2040.idleOtherCore();
    digitalWrite(SW_PIN, HIGH);
    digitalWrite(SW_PIN, LOW);
    rp2040.resumeOtherCore();
}

void loop1()
{
    rp2040.idleOtherCore();
    digitalWrite(SW1_PIN, HIGH);
    digitalWrite(SW1_PIN, LOW);
    rp2040.resumeOtherCore();   
}