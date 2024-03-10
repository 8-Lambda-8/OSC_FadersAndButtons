#include <Arduino.h>

#include "ADS1X15.h"

ADS1115 ADSArray[2];

uint16_t faderValues[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint16_t faderValuesLast[8] = {0, 0, 0, 0, 0, 0, 0, 0};

uint8_t buttonSensePins[] = {32, 33, 34, 35};
uint8_t buttonPullPins[] = {14, 25, 26, 27};
bool buttonArray[sizeof(buttonSensePins) * sizeof(buttonPullPins)];
bool buttonArrayLast[sizeof(buttonArray)];

uint8_t threshold = 16;

void ADS_loop();
bool checkChanged();

void buttonMatrix_loop();

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

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(buttonSensePins[i], INPUT);
    pinMode(buttonPullPins[i], OUTPUT);
  }
}

void loop() {
  ADS_loop();
  buttonMatrix_loop();

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

void buttonMatrix_loop() {
  memcpy(buttonArrayLast, buttonArray, sizeof(buttonArray));

  for (size_t p = 0; p < sizeof(buttonPullPins); p++) {
    for (uint8_t i = 0; i < sizeof(buttonPullPins); i++) digitalWrite(buttonPullPins[i], i != p);
    delay(1);
    for (size_t s = 0; s < sizeof(buttonSensePins); s++) {
      buttonArray[p * sizeof(buttonSensePins) + s] = digitalRead(buttonSensePins[s]);
    }
  }

  for (size_t i = 0; i < sizeof(buttonArray); i++) {
    if (buttonArray[i] != buttonArrayLast[i]) {
      uint8_t y = i / sizeof(buttonPullPins);
      uint8_t x = i % sizeof(buttonPullPins);

      Serial.printf("Button %2d (%d,%d) changed to %s\n", i, x, y,
                    buttonArray[i] ? "Released" : "Pressed");
    }
  }
}
