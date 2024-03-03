#include <Arduino.h>

#include "ADS1X15.h"

ADS1115 ADSArray[2];

uint16_t faderValues[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint16_t faderValuesLast[8] = {0, 0, 0, 0, 0, 0, 0, 0};

uint8_t threshold = 16;

void ADS_loop();
bool checkChanged();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Setup");

  Wire.begin();

  for (size_t i = 0; i < 2; i++) {
    ADSArray[i] = ADS1115(0x48 + i);
    ADSArray[i].begin();
    //  0 = slow   4 = medium   7 = fast but more noise
    ADSArray[i].setDataRate(4);
    for (size_t j = 0; j < 4; j++) ADSArray[i].requestADC(j);
  }
}

void loop() {
  ADS_loop();

  if (checkChanged()) {
    for (size_t i = 0; i < 8; i++) {
      Serial.printf("%5d\t", faderValues[i]);
      faderValuesLast[i] = faderValues[i];
    }
    Serial.println();
  }

  delay(1);
}

uint8_t idx = 0;

void ADS_loop() {
  for (auto &&ADS : ADSArray)
    if (!ADS.isConnected() || ADS.isBusy()) return;

  faderValues[0 + idx] = ADSArray[0].getValue() + 1;
  faderValues[4 + idx] = ADSArray[1].getValue() + 1;

  idx++;
  if (idx > 3) idx = 0;

  for (auto &&ADS : ADSArray) ADS.requestADC(idx);
}

bool checkChanged() {
  bool changed = false;
  for (size_t i = 0; i < 8; i++) {
    if (abs(faderValuesLast[i] - faderValues[i]) > threshold) changed = true;
  }
  return changed;
}