#include <Arduino.h>

HardwareSerial &hartSerial = Serial2;

void setup() {
  Serial.begin(115200);
  hartSerial.begin(1200, SERIAL_8O1, 16, 17);
  Serial.println("Ponte ESP32-HART iniciada. Digite HEX (ex: FF 82 00) e pressione Enter");
}

byte hexToByte(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

void loop() {
  static String inputString = "";

  // Ler comando ASCII do PC
  while (Serial.available()) {
    char inChar = Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      inputString.trim();
      if (inputString.length() > 0) {
        Serial.print("Enviando: ");
        Serial.println(inputString);

        // Converter string hex em bytes reais
        for (int i = 0; i < inputString.length(); i += 3) {
          if (i + 1 < inputString.length()) {
            byte b = (hexToByte(inputString.charAt(i)) << 4) | hexToByte(inputString.charAt(i + 1));
            hartSerial.write(b);
          }
        }
        inputString = "";
      }
    } else {
      inputString += inChar;
    }
  }

  // Ler resposta do HART
  while (hartSerial.available()) {
    byte b = hartSerial.read();
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
    Serial.print(" ");
  }
}