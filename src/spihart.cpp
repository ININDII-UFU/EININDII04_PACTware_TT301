#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "APJosue";
const char* password = "josue32154538";

WebServer server(80);

String logBuffer = "";

HardwareSerial &hartSerial = Serial2;

void logByte(const char* direction, byte b) {
  char hexBuf[5];
  sprintf(hexBuf, "%02X ", b);
  logBuffer += direction;
  logBuffer += hexBuf;
}

void handleRoot() {
  server.send(200, "text/plain", logBuffer);
}

void setup() {
  Serial.begin(115200);
  hartSerial.begin(1200, SERIAL_8O1, 16, 17);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  server.on("/", handleRoot);
  server.begin();

  Serial.println("Servidor de Log iniciado em: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();

  while (Serial.available()) {
    byte b = Serial.read();
    hartSerial.write(b);
    logByte("PC->HART: ", b);
  }

  while (hartSerial.available()) {
    byte b = hartSerial.read();
    Serial.write(b);
    logByte("HART->PC: ", b);
  }
}