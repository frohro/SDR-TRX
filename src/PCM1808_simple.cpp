#include <I2S.h>
#include <Arduino.h>
#include <si5351.h>

#define RATE 48000
#define MCLK_MULT 256

I2S i2s(INPUT);

void setup() {
  Serial.begin();
  i2s.setDATA(2); // These are the pins for the data on the SDR-TRX
  i2s.setBCLK(0);
  i2s.setMCLK(3);
  // Note: LRCK pin is BCK pin plus 1 (1 in this case).
  i2s.setSysClk(RATE);
  i2s.setBitsPerSample(24);
  i2s.setFrequency(RATE);
  i2s.setMCLKmult(MCLK_MULT);
  i2s.setBuffers(32, 0, 0);
  i2s.begin();

  while (1) {
    int32_t l, r;
    i2s.read32(&l, &r);
    l = l << 8;
    r = r << 8;
    Serial.printf("%d %d\r\n", l, r);
  }
}

void loop() {}