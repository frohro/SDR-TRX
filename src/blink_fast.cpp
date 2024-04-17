#include <Arduino.h>
#include <si5351.h>

const int SW_PIN = 16;
const int SW1_PIN = 18;

void setup() 
{
    pinMode(SW_PIN, OUTPUT);
    pinMode(SW1_PIN, OUTPUT);
}
void setup1()
{

}

void loop() {
    Serial.printf("Core 0\n");
    rp2040.idleOtherCore();
    digitalWrite(SW_PIN, HIGH);
    digitalWrite(SW_PIN, LOW);
    rp2040.resumeOtherCore();
}

void loop1()
{
    Serial.printf("Core 1\n");
    rp2040.idleOtherCore();
    digitalWrite(SW1_PIN, HIGH);
    digitalWrite(SW1_PIN, LOW);
    rp2040.resumeOtherCore();   
}