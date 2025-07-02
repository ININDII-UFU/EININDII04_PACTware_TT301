#include <Arduino.h>

HardwareSerial &hartSerial = Serial2;

void setup() {
  Serial.begin(1200, SERIAL_8O1);           // Serial USB (PACTware) — 1200 8O1
  hartSerial.begin(1200, SERIAL_8O1, 16, 17); // UART2 (Modem HART) — 1200 8O1, TX=17, RX=16
}

void loop() {
  // PACTware → HART Modem
  while (Serial.available()) {
    hartSerial.write(Serial.read());
  }
  // HART Modem → PACTware
  while (hartSerial.available()) {
    Serial.write(hartSerial.read());
  }
}