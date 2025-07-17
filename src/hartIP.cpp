// coil status:       [0] def_pin_D2,   [1] def_pin_D3,   [2] def_pin_D4,             [3] def_pin_RELE,           [4]             [5]
// Input status:      [0] def_pin_RTN1, [1] def_pin_RTN2, [2] def_pin_PUSH1,          [3] def_pin_PUSH2,          [4] def_pin_D1, [5]
// Input Registers:   [0] POT1,         [1] POT2,         [2] Leitura 4-20mA canal 1, [3] Leitura 4-20mA canal 2, [4] ADC1,       [5] ADC2,
// Holding Registers: [0] DAC,          [1] Write 4-20mA, [2] PWM                     [3]                         [4]             [5]

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
#include "OTA.h"
#include "display_c.h"
#include "wifimanager_c.h"
#include "ads1115_c.h"

/********** GPIO ***********/
#define def_pin_SDA     21
#define def_pin_SCL     22
#define def_pin_D2      19
#define def_pin_D3      18
#define def_pin_D4      4
#define def_pin_RELE    27
#define def_pin_RTN1    2
#define def_pin_RTN2    35
#define def_pin_PUSH1   34
#define def_pin_PUSH2   32
#define def_pin_D1      23
#define def_pin_ADC1    36
#define def_pin_ADC2    39
#define def_pin_W4a20   26
#define def_pin_PWM     33
#define def_pin_DAC1    25

#define CHANNEL_W4a20   0
#define CHANNEL_PWM     1

/********** HART-IP ***********/
#define HARTIP_PORT           5094
#define HARTIP_CONNECT_REQ    0x01
#define HARTIP_CONNECT_ACK    0x02
#define HARTIP_DATA_REQ       0x03
#define HARTIP_DATA_RESP      0x04
#define HART_PREAMBLE_LEN     5    // número de 0xFF antes do frame HART

struct __attribute__((packed)) HartIPHeader {
  uint16_t length;
  uint8_t  protocolId;
  uint8_t  messageType;
  uint16_t transactionId;
  uint8_t  reserved[4];
};

char DDNSName[16] = "inindkit";
ADS1115_c ads;
WifiManager_c wm;
Display_c disp;
HardwareSerial &hartSerial = Serial2;

WiFiServer hartServer(HARTIP_PORT);
WiFiClient hartClient;
bool sessionOpen = false;    // fica true após receber o CONNECT ACK
uint16_t txId = 1;           // contador de transações HART-IP

// imprime buffer em hex com um prefixo
void logHex(const char* tag, const uint8_t* buf, size_t len) {
  Serial.print(tag);
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// monta e envia um pacote HART-IP (header + payload)
void sendHartIP(uint8_t msgType, const uint8_t* payload = nullptr, uint16_t payloadLen = 0) {
  HartIPHeader h;
  uint16_t total = sizeof(h) + payloadLen;
  h.length        = htons(total);
  h.protocolId    = 0x01;
  h.messageType   = msgType;
  h.transactionId = htons(txId++);
  memset(h.reserved, 0, 4);

  // log header
  logHex(">>> IP TX hdr: ", (uint8_t*)&h, sizeof(h));
  if (payloadLen) logHex(">>> IP TX pld: ", payload, payloadLen);

  // envia tudo
  hartClient.write((uint8_t*)&h, sizeof(h));
  if (payloadLen) hartClient.write(payload, payloadLen);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // UART2 para HART (RX=16, TX=17)
  hartSerial.begin(1200, SERIAL_8O1, 16, 17);

  // monta SSID com ID da EEPROM
  EEPROM.begin(1);
  char idKit = EEPROM.read(0);
  size_t L = strlen(DDNSName);
  DDNSName[L]   = idKit;
  DDNSName[L+1] = '\0';

  // display
  startDisplay(&disp, def_pin_SDA, def_pin_SCL);
  disp.setText(1, "Inicializando...");

  // Wi-Fi STA+AP para config
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

  // pinos I/O
  pinMode(def_pin_D2, OUTPUT);
  pinMode(def_pin_D3, OUTPUT);
  pinMode(def_pin_D4, OUTPUT);
  pinMode(def_pin_RELE, OUTPUT);
  pinMode(def_pin_RTN1, INPUT_PULLDOWN);
  pinMode(def_pin_RTN2, INPUT_PULLDOWN);
  pinMode(def_pin_PUSH1, INPUT_PULLDOWN);
  pinMode(def_pin_PUSH2, INPUT_PULLDOWN);
  pinMode(def_pin_D1, INPUT_PULLDOWN);
  pinMode(def_pin_ADC1, INPUT);
  pinMode(def_pin_ADC2, INPUT);
  pinMode(def_pin_W4a20, OUTPUT);
  pinMode(def_pin_PWM, OUTPUT);
  pinMode(def_pin_DAC1, OUTPUT);

  // PWM
  ledcSetup(CHANNEL_W4a20, 19000, 12);
  ledcAttachPin(def_pin_W4a20, CHANNEL_W4a20);
  ledcWrite(CHANNEL_W4a20, 0);

  ledcSetup(CHANNEL_PWM, 19000, 12);
  ledcAttachPin(def_pin_PWM, CHANNEL_PWM);
  ledcWrite(CHANNEL_PWM, 0);

  // DAC e ADS
  dacWrite(def_pin_DAC1, 0);
  ads.begin();

  // servidor TCP HART-IP
  hartServer.begin();
  Serial.printf("HART-IP TCP server iniciado na porta %d\n", HARTIP_PORT);
}

void loop() {
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();

  // --- 1) aceita conexão apenas uma vez ---
  if (!hartClient || !hartClient.connected()) {
    WiFiClient c = hartServer.available();
    if (c) {
      hartClient = c;
      sessionOpen = false;  // reset
      Serial.printf("PACTware conectou %s:%d\n",
        hartClient.remoteIP().toString().c_str(),
        hartClient.remotePort());
      // envia CONNECT REQUEST
      sendHartIP(HARTIP_CONNECT_REQ);
      Serial.println(">>> CONNECT REQUEST enviado");
    }
  }

  // --- 2) lê do TCP (HART-IP) ---
  if (hartClient && hartClient.connected() && hartClient.available()) {
    // primeiro leio header HART-IP
    HartIPHeader h;
    if (hartClient.readBytes((uint8_t*)&h, sizeof(h)) == sizeof(h)) {
      uint16_t total = ntohs(h.length);
      uint16_t plen  = total - sizeof(h);
      logHex("<<< IP RX hdr: ", (uint8_t*)&h, sizeof(h));
      // leio o payload
      uint8_t payload[256];
      if (plen && plen <= sizeof(payload)) {
        hartClient.readBytes(payload, plen);
        logHex("<<< IP RX pld: ", payload, plen);
      }
      // tratamento conforme tipo
      switch (h.messageType) {
        case HARTIP_CONNECT_ACK:
          Serial.println("<<< CONNECT ACK recebido");
          sessionOpen = true;
          break;
        case HARTIP_DATA_REQ:
          if (sessionOpen) {
            // envia preambulo de 0xFF
            for (int i = 0; i < HART_PREAMBLE_LEN; i++) hartSerial.write(0xFF);
            Serial.printf(">>> enviou %d bytes de preambulo\n", HART_PREAMBLE_LEN);
            // e o frame HART puro
            hartSerial.write(payload, plen);
            logHex(">>> HART TX (raw): ", payload, plen);
          }
          break;
        // ignore keepalive, disconnect etc por enquanto
      }
    }
  }

  // --- 3) lê do modem HART e retorna ao PACTware ---
  if (sessionOpen && hartSerial.available()) {
    static uint8_t buf[256];
    int len = 0;
    unsigned long t0 = millis();
    while (millis() - t0 < 50 && len < (int)sizeof(buf)) {
      if (hartSerial.available()) buf[len++] = hartSerial.read();
    }
    if (len) {
      logHex("<<< HART RAW RX: ", buf, len);
      // encapsula em DATA RESPONSE e envia
      sendHartIP(HARTIP_DATA_RESP, buf, len);
      Serial.println(">>> DATA_RESPONSE enviado");
    }
  }
}