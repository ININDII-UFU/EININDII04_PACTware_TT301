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

/************ GPIO ************/
#define PIN_SDA       21
#define PIN_SCL       22
#define PIN_D2        19
#define PIN_D3        18
#define PIN_D4        4
#define PIN_RELE      27
#define PIN_RTN1      2
#define PIN_RTN2      35
#define PIN_PUSH1     34
#define PIN_PUSH2     32
#define PIN_D1        23
#define PIN_ADC1      36
#define PIN_ADC2      39
#define PIN_W4A20     26
#define PIN_PWM       33
#define PIN_DAC1      25

#define CH_W4A20      0
#define CH_PWM        1

/********** HART-IP ***********/
static const uint16_t HARTIP_PORT        = 5094;
static const uint8_t  HARTIP_PROTO_ID    = 0x01;
static const uint8_t  MSG_CONNECT        = 0x01;
static const uint8_t  MSG_CONNECT_ACK    = 0x02;
static const uint8_t  MSG_DATA_REQ       = 0x03;
static const uint8_t  MSG_DATA_RESP      = 0x04;
static const uint8_t  MSG_KEEPALIVE      = 0x05;
static const uint8_t  MSG_DISCONNECT     = 0x06;
static const uint8_t  MSG_DISCONNECT_ACK = 0x07;

static const uint8_t  PREAMBLE_LEN       = 5;
static const uint32_t KEEPALIVE_MS       = 10000UL;

static const uint8_t  hartChannel        = 1;  // sempre CD=1

static const uint8_t  HARTIP_HDR_LEN     = 12;

char        DDNSName[16] = "inindkit";
ADS1115_c   ads;
WifiManager_c wm;
Display_c   disp;
HardwareSerial &hartSerial = Serial2;

WiFiServer hartServer(HARTIP_PORT);
WiFiClient hartClient;
bool        sessionOpen = false;
uint16_t    txId        = 1;
unsigned long lastKeep  = 0;

// hex-dump para debug
void logHex(const char* label, const uint8_t* buf, size_t len) {
  Serial.print(label);
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// monta e envia HART-IP header + payload
void sendHartIP(uint8_t msgType, const uint8_t* payload = nullptr, uint16_t payloadLen = 0) {
  uint16_t totalLen = HARTIP_HDR_LEN + payloadLen;
  uint8_t hdr[HARTIP_HDR_LEN];

  hdr[0] = (totalLen >> 8) & 0xFF;
  hdr[1] =  totalLen       & 0xFF;
  hdr[2] = HARTIP_PROTO_ID;
  hdr[3] = msgType;
  hdr[4] = (txId >> 8) & 0xFF;
  hdr[5] =  txId       & 0xFF;
  txId++;
  hdr[6] = hartChannel;
  memset(hdr + 7, 0, 5);

  logHex(">>> IP TX hdr: ", hdr, HARTIP_HDR_LEN);
  if (payloadLen) logHex(">>> IP TX pld: ", payload, payloadLen);

  hartClient.write(hdr, HARTIP_HDR_LEN);
  if (payloadLen) hartClient.write(payload, payloadLen);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  hartSerial.begin(1200, SERIAL_8O1, 16, 17);

  EEPROM.begin(1);
  char idKit = EEPROM.read(0);
  size_t pos = strlen(DDNSName);
  DDNSName[pos]   = idKit;
  DDNSName[pos+1] = '\0';

  startDisplay(&disp, PIN_SDA, PIN_SCL);
  disp.setText(1, "Inicializando...");

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

  OTA::start(DDNSName);

  pinMode(PIN_D2,    OUTPUT);
  pinMode(PIN_D3,    OUTPUT);
  pinMode(PIN_D4,    OUTPUT);
  pinMode(PIN_RELE,  OUTPUT);
  pinMode(PIN_RTN1,  INPUT_PULLDOWN);
  pinMode(PIN_RTN2,  INPUT_PULLDOWN);
  pinMode(PIN_PUSH1, INPUT_PULLDOWN);
  pinMode(PIN_PUSH2, INPUT_PULLDOWN);
  pinMode(PIN_D1,    INPUT_PULLDOWN);
  pinMode(PIN_ADC1,  INPUT);
  pinMode(PIN_ADC2,  INPUT);
  pinMode(PIN_W4A20, OUTPUT);
  pinMode(PIN_PWM,   OUTPUT);
  pinMode(PIN_DAC1,  OUTPUT);

  ledcSetup(CH_W4A20, 19000, 12);
  ledcAttachPin(PIN_W4A20, CH_W4A20);
  ledcWrite(CH_W4A20, 0);
  ledcSetup(CH_PWM,   19000, 12);
  ledcAttachPin(PIN_PWM, CH_PWM);
  ledcWrite(CH_PWM,   0);
  dacWrite(PIN_DAC1,  0);

  ads.begin();

  hartServer.begin();
  Serial.printf("HART-IP TCP server iniciado na porta %u\n", HARTIP_PORT);
}

void loop() {
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();

  // 1) aceita nova conexão
  if ((!hartClient || !hartClient.connected()) && hartServer.hasClient()) {
    hartClient = hartServer.available();
    sessionOpen = false;
    Serial.printf("PACTware conectou %s:%u\n",
                  hartClient.remoteIP().toString().c_str(),
                  hartClient.remotePort());
    sendHartIP(MSG_CONNECT);
    Serial.println(">>> CONNECT REQUEST enviado");
  }

  // 2) keepalive
  if (sessionOpen && millis() - lastKeep > KEEPALIVE_MS) {
    sendHartIP(MSG_KEEPALIVE);
    lastKeep = millis();
    Serial.println(">>> KEEPALIVE enviado");
  }

  // 3) processa HART-IP recebido
  if (hartClient && hartClient.connected()) {
    if (hartClient.available() >= HARTIP_HDR_LEN) {
      uint8_t hdr[HARTIP_HDR_LEN];
      hartClient.readBytes(hdr, HARTIP_HDR_LEN);

      if (hdr[2] != HARTIP_PROTO_ID) {
        Serial.printf("!!! protocolo inesperado: 0x%02X\n", hdr[2]);
        hartClient.stop();
        return;
      }

      uint16_t totalLen   = (hdr[0]<<8) | hdr[1];
      uint8_t  msgType    = hdr[3];
      uint16_t payloadLen = totalLen - HARTIP_HDR_LEN;

      logHex("<<< IP RX hdr: ", hdr, HARTIP_HDR_LEN);

      uint8_t payload[512];
      if (payloadLen && payloadLen <= sizeof(payload)) {
        // aguarda payload completo
        while (hartClient.available() < payloadLen) { delay(1); }
        hartClient.readBytes(payload, payloadLen);
        logHex("<<< IP RX pld: ", payload, payloadLen);
      }

      switch (msgType) {
        case MSG_CONNECT_ACK:
          Serial.println("<<< CONNECT ACK recebido");
          sessionOpen = true;
          lastKeep    = millis();
          break;

        case MSG_DATA_REQ:
          if (sessionOpen && payloadLen) {
            for (int i = 0; i < PREAMBLE_LEN; i++) hartSerial.write(0xFF);
            Serial.printf(">>> Enviado %d×0xFF\n", PREAMBLE_LEN);
            hartSerial.write(payload, payloadLen);
            logHex(">>> HART TX (raw): ", payload, payloadLen);
          }
          break;

        case MSG_DISCONNECT:
          Serial.println("<<< DISCONNECT recebido");
          sendHartIP(MSG_DISCONNECT_ACK);
          hartClient.stop();
          sessionOpen = false;
          break;
      }
    }
  }

  // 4) recebe HART do modem e reenvia
  if (sessionOpen && hartSerial.available()) {
    static uint8_t buf[256];
    int len = 0;
    unsigned long t0 = millis();
    while (millis() - t0 < 50 && len < (int)sizeof(buf)) {
      if (hartSerial.available()) buf[len++] = hartSerial.read();
    }
    if (len > 0) {
      int idx = 0;
      while (idx < PREAMBLE_LEN && buf[idx] == 0xFF) idx++;
      int actual = len - idx;
      if (actual > 0) {
        logHex("<<< HART RAW RX: ", buf+idx, actual);
        sendHartIP(MSG_DATA_RESP, buf+idx, actual);
        Serial.println(">>> DATA_RESP enviado");
      }
    }
  }
}