#include <Arduino.h>
#include "Ethernet.h"
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include "ADS1X15.h"

#include "buttonMatrix.h"

// Ethernet
uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48};  // MAC Adress of your device
IPAddress ip(10, 0, 0, 123);                           // IP address of your device
IPAddress dns(10, 0, 0, 1);                            // DNS address of your device

IPAddress console(10, 0, 0, 5);

EthernetUDP Udp;
const unsigned int console_Port = 9999;
const unsigned int localPort = 8888;

ADS1115 ADSArray[2];

uint16_t faderValues[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint16_t faderValuesLast[8] = {0, 0, 0, 0, 0, 0, 0, 0};

uint8_t buttonSensePins[] = {32, 33, 34, 35};
uint8_t buttonPullPins[] = {14, 25, 26, 27};
ButtonMatrix buttonMatrix(buttonSensePins, buttonPullPins, sizeof(buttonSensePins),
                          sizeof(buttonPullPins));

void valueChangedCallback(uint8_t i, bool state) {
  Serial.printf("Button %2d (%d,%d) changed to %s\n", i, buttonMatrix.getX(i), buttonMatrix.getY(i),
                state ? "Released" : "Pressed");
  sendOscMessage("button/" + i, state ? 1.0 : 0.0);
};

uint8_t threshold = 16;

void ADS_loop();
bool checkChanged();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Setup");

  buttonMatrix.valueChangedCallback(valueChangedCallback);

  // init Ethernet
  Ethernet.init(5);
  while (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("hardware Error");
  if (Ethernet.begin(mac) == 0) {
    delay(10);
    Ethernet.begin(mac, ip, dns);
  }
  Serial.println(Ethernet.localIP());
  Udp.begin(localPort);


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
  buttonMatrix.loop();

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

void sendOscMessage(const String &address, float value) {
  OSCMessage msg(address.c_str());
  msg.add(value);
  Udp.beginPacket(console_ip, console_Port);
  msg.send(Udp);
  Udp.endPacket();
}
