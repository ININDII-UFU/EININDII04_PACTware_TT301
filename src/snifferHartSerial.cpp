#include <Arduino.h>
#include <WiFi.h>

// --- CONFIGURE SEU WIFI ---
const char *ssid = "APJosue";
const char *password = "josue32154538";
const IPAddress local_ip(200, 19, 148, 112);
const IPAddress gateway(200, 19, 148, 1);
const IPAddress subnet(255, 255, 255, 0);
// Porta para log TCP
#define LOG_PORT 5000

WiFiServer logServer(LOG_PORT);
WiFiClient logClient; // Apenas um cliente para log

HardwareSerial &hartSerial = Serial2;

void send_log(const char *prefix, const uint8_t *data, size_t len) {
  if (logClient && logClient.connected()) {
    logClient.write((const uint8_t*)prefix, strlen(prefix));
    logClient.write(data, len);
    logClient.write("\n", 1); // delimita
  }
}

void printHex(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void setup() {
  Serial.begin(1200, SERIAL_8O1);           // Serial USB (PACTware)
  hartSerial.begin(1200, SERIAL_8O1, 16, 17); // UART2 (Modem HART)

  // Setup WiFi
  Serial2.begin(115200); // Para debug na Serial2 se precisar (opcional)
  Serial2.println("\n=== ESP32 Serial ↔ HART Modem + TCP LOG ===");
  WiFi.mode(WIFI_STA);
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  logServer.begin();
  Serial2.print("WiFi OK. IP: ");
  Serial2.println(WiFi.localIP());
  Serial2.println("Aguardando conexão TCP para LOG na porta 5000...");
}

void loop() {
  // Aceita conexão de log (uma vez)
  if (!logClient || !logClient.connected()) {
    logClient = logServer.available();
  }

  // PACTware → HART Modem
  while (Serial.available()) {
    uint8_t b = Serial.read();
    hartSerial.write(b);
    send_log("[TX] ", &b, 1);
    printHex(&b, 1);
  }

  // HART Modem → PACTware
  while (hartSerial.available()) {
    uint8_t b = hartSerial.read();
    Serial.write(b);
    send_log("[RX] ", &b, 1);
    printHex(&b, 1);
  }
}