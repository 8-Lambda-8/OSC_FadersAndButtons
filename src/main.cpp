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
const uint8_t columns = sizeof(buttonSensePins);
const uint8_t rows = sizeof(buttonPullPins);
const uint8_t buttonCount = columns * rows;
ButtonMatrix buttonMatrix(buttonSensePins, buttonPullPins, columns, rows);

Faders faders(2);
float maxFaderVal = 24000.0;  // 17400.0;

NeoPixelBus<NeoGrbFeature, NeoWs2812Method> leds(buttonCount, 4);
uint8_t feedback[buttonCount];
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
                state ? "Pressed" : "Released");

  sprintf(formatBuffer, "/button/%d", i);
  sendOscMessage(formatBuffer, state ? 1.0 : 0.0);
};

void faderChangedCallback(uint8_t i, u_int16_t value) {
  if (value > (2 * maxFaderVal)) return;
  Serial.printf("Fader %2d %5d -> %.4f\n", i, value, value / maxFaderVal);
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
      leds.SetPixelColor(ledMap(i + columns * 0), col);
      leds.SetPixelColor(ledMap(i + columns * 1), col);
      leds.SetPixelColor(ledMap(i + columns * 2), col);
      leds.SetPixelColor(ledMap(i + columns * 3), col);
      leds.Show();
      delay(30);
    }

  buttonMatrix.init();
  buttonMatrix.valueChangedCallback(buttonChangedCallback);
  faders.init();
  faders.valueChangedCallback(faderChangedCallback);

  // init Ethernet
  Ethernet.init(5);
  if (Ethernet.begin(mac) == 0) {
    while (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("hardware Error");
      delay(10);
      Ethernet.begin(mac, ip, dns);
    }
  }
  Serial.println(Ethernet.localIP());
  Udp.begin(localPort);
}

void routeButton(OSCMessage& msg, int addrOffset) {
  if (msg.isFloat(0)) {
    addrOffset++;
    uint8_t colId = (uint8_t)(msg.getFloat(0) * 255.0);
    uint8_t btnId = atoi(msg.getAddress() + addrOffset);
    feedback[btnId] = colId;
    Serial.printf("L %2d %2d\n", btnId, colId);
  }
}

uint32_t ledTimer = 0;
uint8_t blinkState = 0;

void loop() {
  faders.loop();
  buttonMatrix.loop();

  OSCMessage msgIN;
  int size;
  if ((size = Udp.parsePacket()) > 0) {
    while (size--) msgIN.fill(Udp.read());
    if (!msgIN.hasError()) {
      msgIN.route("/button", routeButton);
    } else {
      Serial.println("error");
    }
  }

  if (ledTimer < millis() - 50) {
    for (size_t i = 0; i < buttonCount; i++) {
      uint8_t fb = feedback[i];
      if (fb < 14)  // static
        leds.SetPixelColor(ledMap(i), colors[fb]);
      else if (fb < 27)  // fast Blink
        leds.SetPixelColor(ledMap(i), colors[((blinkState / 2) % 2) ? fb - 13 : 0]);
      else if (fb < 39)  // middle Blink
        leds.SetPixelColor(ledMap(i), colors[((blinkState / 4) % 2) ? fb - 26 : 0]);
      else if (fb < 51)  // slow Blink
        leds.SetPixelColor(ledMap(i), colors[((blinkState / 8) % 2) ? fb - 38 : 0]);
    }

    blinkState++;
    if (blinkState > 16) blinkState = 0;
    leds.Show();
    ledTimer = millis();
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
