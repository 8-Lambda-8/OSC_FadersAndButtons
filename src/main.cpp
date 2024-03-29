#include <Arduino.h>
#include "Ethernet.h"
#include <EthernetUdp.h>
#include <OSCMessage.h>
#include <OSCBoards.h>

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
RgbColor colors[] = {
    RgbColor(0x00),              // 00   OFF
    RgbColor(0xff),              // 01   WHITE
    RgbColor(0xff, 0x00, 0x00),  // 02   RED
    RgbColor(0xff, 0x80, 0x00),  // 03   Orange
    RgbColor(0xff, 0xff, 0x00),  // 04   Yellow
    RgbColor(0x80, 0xff, 0x00),  // 05   Charteuse
    RgbColor(0x00, 0xaa, 0x00),  // 06   GREEN
    RgbColor(0x00, 0xff, 0x80),  // 07   Spring Green
    RgbColor(0x00, 0xff, 0xff),  // 08   Cyan
    RgbColor(0x00, 0x80, 0xff),  // 09   Azure
    RgbColor(0x00, 0x00, 0xff),  // 10   BLUE
    RgbColor(0x80, 0x00, 0xff),  // 11   Electric Indigo
    RgbColor(0xff, 0x00, 0xff),  // 12   Magenta
    RgbColor(0xff, 0x00, 0x80),  // 13   Rose
};

uint8_t ledMap(uint8_t button);
void sendOscMessage(char* address, float value);

char formatBuffer[20];

void buttonChangedCallback(uint8_t i, bool state) {
  Serial.printf("Button %2d (%d,%d) changed to %s\n", i, buttonMatrix.getX(i), buttonMatrix.getY(i),
                state ? "Released" : "Pressed");

  sprintf(formatBuffer, "/button/%d", i);
  sendOscMessage(formatBuffer, state ? 1.0 : 0.0);
};

void faderChangedCallback(uint8_t i, u_int16_t value) {
  sprintf(formatBuffer, "/fader/%d", i);
  sendOscMessage(formatBuffer, value / maxFaderVal);
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Setup");

  leds.Begin();
  for (auto&& col : {RgbColor(128, 0, 0), RgbColor(0, 128, 0), RgbColor(0, 0, 128), colors[0]})
    for (size_t i = 0; i < 8; i++) {
      leds.SetPixelColor(ledMap(i + 8 * 0), col);
      leds.SetPixelColor(ledMap(i + 8 * 1), col);
      leds.SetPixelColor(ledMap(i + 8 * 2), col);
      leds.SetPixelColor(ledMap(i + 8 * 3), col);
      leds.Show();
      delay(30);
    }

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

void routeButton(OSCMessage& msg, int addrOffset) {
  if (msg.isFloat(0)) {
    uint8_t colId = (uint8_t)(msg.getFloat(0) * 255.0);
    leds.SetPixelColor(ledMap(atoi(msg.getAddress() + addrOffset)), colors[colId]);
    leds.Show();
  }
}

void loop() {
  faders.loop();
  buttonMatrix.loop();

  OSCMessage msgIN;
  int size;
  if ((size = Udp.parsePacket()) > 0) {
    Serial.print("UDP rec ");
    Serial.println(size);

    while (size--) msgIN.fill(Udp.read());
    if (!msgIN.hasError()) {
      msgIN.route("/button", routeButton);

    } else {
      Serial.println("error");
    }
  }
}

void sendOscMessage(char* address, float value) {
  OSCMessage msg(address);
  msg.add(value);

  Udp.beginPacket(console, console_Port);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

uint8_t ledMap(uint8_t button) {
  uint8_t mod = button % 16;
  if (mod < 4) return button;
  if (mod < 8) return button + 4;
  if (mod < 12) return button - 4;
  return button;
}
