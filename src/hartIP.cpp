// coil status:       [0] def_pin_D2,   [1] def_pin_D3,   [2] def_pin_D4,             [3] def_pin_RELE,           [4]             [5]
// Input status:      [0] def_pin_RTN1, [1] def_pin_RTN2, [2] def_pin_PUSH1,          [3] def_pin_PUSH2,          [4] def_pin_D1, [5]
// Input Registers:   [0] POT1,         [1] POT2,         [2] Leitura 4-20mA canal 1, [3] Leitura 4-20mA canal 2, [4] ADC1,       [5] ADC2,
// Holding Registers: [0] DAC,          [1] Write 4-20mA, [2] PWM                     [3]                         [4]             [5]

#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include "OTA.h"
#include "display_c.h"
#include "wifimanager_c.h"
#include "ads1115_c.h"

/********** GPIO ***********/
#define def_pin_SDA    21
#define def_pin_SCL    22
#define def_pin_D2     19
#define def_pin_D3     18
#define def_pin_D4     4
#define def_pin_RELE   27
#define def_pin_RTN1   2
#define def_pin_RTN2   35
#define def_pin_PUSH1  34
#define def_pin_PUSH2  32
#define def_pin_D1     23
#define def_pin_ADC1   36
#define def_pin_ADC2   39
#define def_pin_W4a20  26
#define def_pin_PWM    33
#define def_pin_DAC1   25

#define CHANNEL_W4a20  0
#define CHANNEL_PWM    1

/********** HART-IP ***********/
#define HARTIP_PORT           5094
#define HARTIP_CONNECT_REQ    0x01
#define HARTIP_CONNECT_ACK    0x02
#define HARTIP_DATA_REQ       0x03
#define HARTIP_DATA_RESP      0x04
#define HARTIP_KEEPALIVE      0x05
#define HARTIP_DISCONNECT     0x06
#define HARTIP_DISCONNECT_ACK 0x07

#define HART_PREAMBLE_LEN     5     // até 5 bytes 0xFF
#define KEEPALIVE_INTERVAL    10000UL

// Selecione sempre Channel 1 no DTM → hdr[6] = 1
static const uint8_t hartChannel = 1;

char     DDNSName[16] = "inindkit";
ADS1115_c ads;
WifiManager_c wm;
Display_c   disp;
HardwareSerial &hartSerial = Serial2;

WiFiServer   hartServer(HARTIP_PORT);
WiFiClient   hartClient;
bool         sessionOpen = false;
uint16_t     txId        = 1;
unsigned long lastKeep   = 0;

// hex-dump para debug
void logHex(const char* tag, const uint8_t* buf, size_t len) {
  Serial.print(tag);
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// Monta e envia o header HART-IP (12 bytes) + payload
void sendHartIP(uint8_t msgType, const uint8_t* payload = nullptr, uint16_t payloadLen = 0) {
  const uint8_t HEADER_LEN = 12;
  uint16_t totalLen = HEADER_LEN + payloadLen;
  uint8_t hdr[HEADER_LEN];

  // bytes 0–1: comprimento big-endian
  hdr[0] = (totalLen >> 8) & 0xFF;
  hdr[1] = totalLen & 0xFF;
  hdr[2] = 0x01;               // protocol ID
  hdr[3] = msgType;            // tipo de mensagem
  hdr[4] = (txId >> 8) & 0xFF; // transaction ID MSB
  hdr[5] = txId & 0xFF;        // transaction ID LSB
  txId++;
  hdr[6] = hartChannel;        // Channel (1)
  memset(hdr + 7, 0, 5);       // reserved

  logHex(">>> IP TX hdr: ", hdr, HEADER_LEN);
  if (payloadLen) logHex(">>> IP TX pld: ", payload, payloadLen);

  hartClient.write(hdr, HEADER_LEN);
  if (payloadLen) hartClient.write(payload, payloadLen);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // UART2 para modem HART (RX=16, TX=17)
  hartSerial.begin(1200, SERIAL_8O1, /*RX=*/16, /*TX=*/17);

  // Leitura de ID da EEPROM para SSID
  EEPROM.begin(1);
  char idKit = EEPROM.read(0);
  size_t L = strlen(DDNSName);
  DDNSName[L]   = idKit;
  DDNSName[L+1] = '\0';

  // Display
  startDisplay(&disp, def_pin_SDA, def_pin_SCL);
  disp.setText(1, "Inicializando...");

  // Wi-Fi STA + AP para configuração
  WiFi.mode(WIFI_STA);
  wm.setApName(DDNSName);
  disp.setFuncMode(true);
  disp.setText(1, "Mode: AP");
  disp.setText(2, "SSID: AutoConnectAP");
  disp.setText(3, "PSWD: N/A");
  if (wm.autoConnect("AutoConnectAP")) {
    disp.setFuncMode(false);
    disp.setText(1, WiFi.localIP().toString().c_str());
    disp.setText(2, DDNSName);
    disp.setText(3, "UFU Mode");
    delay(50);
  }

  // OTA
  OTA::start(DDNSName);

  // Configura I/O
  pinMode(def_pin_D2,    OUTPUT);
  pinMode(def_pin_D3,    OUTPUT);
  pinMode(def_pin_D4,    OUTPUT);
  pinMode(def_pin_RELE,  OUTPUT);
  pinMode(def_pin_RTN1,  INPUT_PULLDOWN);
  pinMode(def_pin_RTN2,  INPUT_PULLDOWN);
  pinMode(def_pin_PUSH1, INPUT_PULLDOWN);
  pinMode(def_pin_PUSH2, INPUT_PULLDOWN);
  pinMode(def_pin_D1,    INPUT_PULLDOWN);
  pinMode(def_pin_ADC1,  INPUT);
  pinMode(def_pin_ADC2,  INPUT);
  pinMode(def_pin_W4a20, OUTPUT);
  pinMode(def_pin_PWM,   OUTPUT);
  pinMode(def_pin_DAC1,  OUTPUT);

  // PWM e DAC
  ledcSetup(CHANNEL_W4a20, 19000, 12);
  ledcAttachPin(def_pin_W4a20, CHANNEL_W4a20);
  ledcWrite(CHANNEL_W4a20, 0);

  ledcSetup(CHANNEL_PWM, 19000, 12);
  ledcAttachPin(def_pin_PWM, CHANNEL_PWM);
  ledcWrite(CHANNEL_PWM, 0);

  dacWrite(def_pin_DAC1, 0);
  ads.begin();

  // Inicia servidor TCP HART-IP
  hartServer.begin();
  Serial.printf("HART-IP TCP server iniciado na porta %d\n", HARTIP_PORT);
}

void loop() {
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();

  // 1) Aceita conexão apenas 1 vez
  if ((!hartClient || !hartClient.connected()) && hartServer.hasClient()) {
    hartClient = hartServer.available();
    sessionOpen = false;
    Serial.printf("PACTware conectou %s:%d\n",
                  hartClient.remoteIP().toString().c_str(),
                  hartClient.remotePort());
    sendHartIP(HARTIP_CONNECT_REQ);
    Serial.println(">>> CONNECT REQUEST enviado");
  }

  // 2) Keep-alive se sessão aberta
  if (sessionOpen && millis() - lastKeep > KEEPALIVE_INTERVAL) {
    sendHartIP(HARTIP_KEEPALIVE);
    lastKeep = millis();
    Serial.println(">>> KEEPALIVE enviado");
  }

  // 3) Processa HART-IP vindo do PACTware
  if (hartClient && hartClient.connected()) {
    const uint8_t HEADER_LEN = 12;
    if (hartClient.available() >= HEADER_LEN) {
      uint8_t hdr[HEADER_LEN];
      hartClient.readBytes(hdr, HEADER_LEN);
      uint16_t totalLen   = (hdr[0] << 8) | hdr[1];
      uint8_t  msgType    = hdr[3];
      uint16_t payloadLen = totalLen - HEADER_LEN;

      logHex("<<< IP RX hdr: ", hdr, HEADER_LEN);

      uint8_t payload[512];
      if (payloadLen && payloadLen <= sizeof(payload)) {
        hartClient.readBytes(payload, payloadLen);
        logHex("<<< IP RX pld: ", payload, payloadLen);
      }

      // Trata tipos
      switch (msgType) {
        case HARTIP_CONNECT_ACK:
          Serial.println("<<< CONNECT ACK recebido");
          sessionOpen = true;
          lastKeep = millis();
          break;

        case HARTIP_DATA_REQ:
          if (sessionOpen && payloadLen) {
            // envia preâmbulo 0xFF×5
            for (int i = 0; i < HART_PREAMBLE_LEN; i++) hartSerial.write(0xFF);
            Serial.printf(">>> Enviado %d bytes de preâmbulo\n", HART_PREAMBLE_LEN);
            // envia quadro HART cru
            hartSerial.write(payload, payloadLen);
            logHex(">>> HART TX (raw): ", payload, payloadLen);
          }
          break;

        case HARTIP_DISCONNECT:
          Serial.println("<<< DISCONNECT recebido");
          sendHartIP(HARTIP_DISCONNECT_ACK);
          hartClient.stop();
          sessionOpen = false;
          break;
      }
    }
  }

  // 4) Processa resposta do modem HART → DATA_RESP
  if (sessionOpen && hartSerial.available()) {
    static uint8_t raw[256];
    int len = 0;
    unsigned long t0 = millis();
    // agrupa bytes por 50 ms
    while (millis() - t0 < 50 && len < (int)sizeof(raw)) {
      if (hartSerial.available()) raw[len++] = hartSerial.read();
    }
    if (len > 0) {
      // descarta até 5 pré-ambles 0xFF
      int start = 0;
      while (start < len && start < HART_PREAMBLE_LEN && raw[start] == 0xFF) {
        start++;
      }
      int actualLen = len - start;
      if (actualLen > 0) {
        logHex("<<< HART RAW RX (stripped): ", raw + start, actualLen);
        sendHartIP(HARTIP_DATA_RESP, raw + start, actualLen);
        Serial.println(">>> DATA_RESPONSE enviado (sem preâmbulo)");
      }
    }
  }
}