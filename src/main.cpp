#include <Arduino.h>
#include "Ethernet.h"
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include <NeoPixelBus.h>

#include "i2c_fader.h"
#include "buttonMatrix.h"

// Ethernet
uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48};  // MAC Adress of your device
IPAddress ip(10, 0, 0, 123);                           // IP address of your device
IPAddress dns(10, 0, 0, 1);                            // DNS address of your device

IPAddress console(10, 0, 0, 5);

EthernetUDP Udp;
const unsigned int console_Port = 9999;
const unsigned int localPort = 8888;

uint8_t buttonSensePins[] = {26, 25, 33, 32, 35, 34, 39, 36};
uint8_t buttonPullPins[] = {13, 12, 14, 27};
ButtonMatrix buttonMatrix(buttonSensePins, buttonPullPins, sizeof(buttonSensePins),
                          sizeof(buttonPullPins));

Faders faders(2);

NeoPixelBus<NeoGrbFeature, NeoWs2812Method> leds(32, 4);

void sendOscMessage(const String &address, float value);

void buttonChangedCallback(uint8_t i, bool state) {
  Serial.printf("Button %2d (%d,%d) changed to %s\n", i, buttonMatrix.getX(i), buttonMatrix.getY(i),
                state ? "Released" : "Pressed");
  sendOscMessage("button/" + i, state ? 1.0 : 0.0);
};

void faderChangedCallback(uint8_t i, u_int16_t value) {
  Serial.printf("Fader %2d changed to %5d\n", i, value);
  sendOscMessage("fader/" + i, value / 1024.0);
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Setup");

  leds.Begin();
  for (size_t i = 0; i < 32; i++) {
    leds.SetPixelColor(i, RgbColor(30,0,0));
  }
  leds.Show();

  buttonMatrix.init();
  buttonMatrix.valueChangedCallback(buttonChangedCallback);
  faders.init();
  faders.valueChangedCallback(faderChangedCallback);

  // init Ethernet
  Ethernet.init(5);
  while (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("hardware Error");
  if (Ethernet.begin(mac) == 0) {
    delay(10);
    Ethernet.begin(mac, ip, dns);
  }
  Serial.println(Ethernet.localIP());
  Udp.begin(localPort);
}

void loop() {
  faders.loop();
  buttonMatrix.loop();

  delay(1);
}

void sendOscMessage(const String &address, float value) {
  OSCMessage msg(address.c_str());
  msg.add(value);
  Udp.beginPacket(console.toString().c_str(), console_Port);
  msg.send(Udp);
  Udp.endPacket();
}
