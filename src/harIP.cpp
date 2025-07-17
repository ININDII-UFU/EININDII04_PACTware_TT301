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

/********** Definições dos GPIO ***********/
// Entradas analógicas e digitais
#define def_pin_ADC1        36    // ADC canal 1 (GPIO36)
#define def_pin_ADC2        39    // ADC canal 2 (GPIO39)
#define def_pin_RTN2        35    // Input retorno 2 (GPIO35)
#define def_pin_PUSH1       34    // Botão 1 (GPIO34)
#define def_pin_PWM         33    // Saída PWM (GPIO33)
#define def_pin_PUSH2       32    // Botão 2 (GPIO32)
#define def_pin_RELE        27    // Relê (GPIO27)
#define def_pin_W4a20_1     26    // Saída 4–20 mA (GPIO26 via PWM)
#define def_pin_DAC1        25    // Saída DAC (GPIO25)
#define def_pin_D1          23    // Input digital extra (GPIO23)
#define def_pin_SCL         22    // I²C SCL (GPIO22)
#define def_pin_SDA         21    // I²C SDA (GPIO21)
#define def_pin_D2          19    // Saída digital 2 (GPIO19)
#define def_pin_D3          18    // Saída digital 3 (GPIO18)
#define def_pin_D4          4     // Saída digital 4 (GPIO4)
#define def_pin_RTN1        2     // Input retorno 1 (GPIO2)

#define CHANNEL_W4a20       0     // Canal LEDC para 4–20 mA
#define CHANNEL_PWM         1     // Canal LEDC para PWM

/********** HART-IP Definitions ***********/
// Tipos de mensagem HART-IP
#define HARTIP_CONNECT_REQ     0x01
#define HARTIP_CONNECT_ACK     0x02
#define HARTIP_DATA_REQ        0x03
#define HARTIP_DATA_RESP       0x04
#define HARTIP_KEEPALIVE       0x05
#define HARTIP_DISCONNECT      0x06
#define HARTIP_DISCONNECT_ACK  0x07

// Estrutura de cabeçalho HART-IP (7+1 bytes de reservado = 12 bytes)
struct __attribute__((packed)) HartIPHeader {
  uint16_t length;        // tamanho total: header + payload, em network order
  uint8_t  protocolId;    // sempre 0x01
  uint8_t  messageType;   // um dos HARTIP_* acima
  uint16_t transactionId; // incrementa a cada envio, em network order
  uint8_t  reserved[4];   // zeros
};

char DDNSName[16] = "inindkit";     // Base do nome do AP
ADS1115_c ads;                      // Conversor 4-20 mA / ADC I2C
WifiManager_c wm;                   // Gerenciador de Wi-Fi auto-connect
Display_c disp;                     // Display OLED
HardwareSerial &hartSerial = Serial2;  // UART2 → modem HART (TX=17, RX=16)

// HART-IP via TCP
const uint16_t HARTIP_PORT = 5094;
WiFiServer hartServer(HARTIP_PORT);
WiFiClient hartClient;
uint16_t txId = 1;  // contador de transações HART-IP

// Função genérica para enviar envelope HART-IP
void sendHartIP(WiFiClient &client, uint8_t msgType, const uint8_t *payload, uint16_t payloadLen) {
  HartIPHeader h;
  uint16_t totalLen = sizeof(h) + payloadLen;
  h.length        = htons(totalLen);  // converte para big-endian
  h.protocolId    = 0x01;             // sempre 1
  h.messageType   = msgType;          // tipo da mensagem
  h.transactionId = htons(txId++);    // incrementa ID de transação
  memset(h.reserved, 0, sizeof(h.reserved));  // zera reservado

  client.write((uint8_t*)&h, sizeof(h));      // envia header
  if (payloadLen > 0) {
    client.write(payload, payloadLen);        // envia dados HART brutais
  }
}

void setup() {
  Serial.begin(115200);
  // Configura UART2 para o modem HART
  hartSerial.begin(1200, SERIAL_8O1, /*RX=*/16, /*TX=*/17);
  delay(100);

  // Lê ID do kit da EEPROM e anexa ao DDNSName
  EEPROM.begin(1);
  char idKit = EEPROM.read(0);              // byte armazenado
  size_t n = strlen(DDNSName);
  DDNSName[n]   = idKit;                    
  DDNSName[n+1] = '\0';

  // Inicializa display I2C
  startDisplay(&disp, def_pin_SDA, def_pin_SCL);
  disp.setText(1, "Inicializando...");

  // Configura Wi-Fi em modo STA+AP p/ configuração inicial
  WiFi.mode(WIFI_STA);
  wm.setApName(DDNSName);
  disp.setFuncMode(true);
  disp.setText(1, "Mode: AP");
  disp.setText(2, "SSID: AutoConnectAP");
  disp.setText(3, "PSWD: N/A");
  if (wm.autoConnect("AutoConnectAP")) {
    // após conectar, mostra IP e nome
    disp.setFuncMode(false);
    disp.setText(1, WiFi.localIP().toString().c_str());
    disp.setText(2, DDNSName);
    disp.setText(3, "UFU Mode");
    delay(50);
  }

  // Inicializa OTA
  OTA::start(DDNSName);

  // Configura pinos digitais e analógicos
  pinMode(def_pin_ADC1, INPUT);
  pinMode(def_pin_ADC2, INPUT);
  pinMode(def_pin_DAC1, OUTPUT);
  pinMode(def_pin_D2, OUTPUT);
  pinMode(def_pin_D3, OUTPUT);
  pinMode(def_pin_D4, OUTPUT);
  pinMode(def_pin_RELE, OUTPUT);
  pinMode(def_pin_W4a20_1, OUTPUT);
  pinMode(def_pin_PWM, OUTPUT);
  pinMode(def_pin_RTN1, INPUT_PULLDOWN);
  pinMode(def_pin_RTN2, INPUT_PULLDOWN);
  pinMode(def_pin_PUSH1, INPUT_PULLDOWN);
  pinMode(def_pin_PUSH2, INPUT_PULLDOWN);
  pinMode(def_pin_D1, INPUT_PULLDOWN);

  // Configura canais PWM para 4–20 mA e PWM
  ledcSetup(CHANNEL_W4a20, 19000, 12);
  ledcAttachPin(def_pin_W4a20_1, CHANNEL_W4a20);
  ledcWrite(CHANNEL_W4a20, 0);
  ledcSetup(CHANNEL_PWM, 19000, 12);
  ledcAttachPin(def_pin_PWM, CHANNEL_PWM);
  ledcWrite(CHANNEL_PWM, 0);

  // Zera saída DAC e inicia ADC I2C
  dacWrite(def_pin_DAC1, 0);
  ads.begin();

  // Inicia servidor TCP HART-IP na porta 5094
  hartServer.begin();
  Serial.printf("HART-IP TCP server iniciado na porta %u\n", HARTIP_PORT);
}

void loop() {
  // Mantém OTA, display e portal de configuração
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();

  // 1) Aceita nova conexão do PACTware se não houver cliente ativo
  if (!hartClient || !hartClient.connected()) {
    WiFiClient newClient = hartServer.available();
    if (newClient) {
      if (hartClient) hartClient.stop();  // fecha cliente anterior
      hartClient = newClient;
      Serial.printf("PACTware conectou de %s:%u\n",
        hartClient.remoteIP().toString().c_str(),
        hartClient.remotePort());

      // 1.a) Envia CONNECT REQUEST p/ iniciar sessão HART-IP
      sendHartIP(hartClient, HARTIP_CONNECT_REQ, nullptr, 0);
      Serial.println("Enviado CONNECT REQUEST");
      // (aqui idealmente leríamos e validamos o CONNECT ACK antes de prosseguir)
    }
  }

  // 2) Quando cliente conectado, lê TCP → envia ao modem HART
  if (hartClient && hartClient.connected() && hartClient.available()) {
    static uint8_t buf[256];
    int len = hartClient.read(buf, sizeof(buf));
    if (len > 0) {
      Serial.printf("TCP→HART %d bytes\n", len);
      hartSerial.write(buf, len);
    }
  }

  // 3) Lê resposta do modem HART (raw) e envia como DATA RESPONSE
  if (hartSerial.available()) {
    static uint8_t responseBuf[256];
    int respLen = 0;
    unsigned long start = millis();
    // agrupa até 50 ms de bytes
    while (millis() - start < 50 && respLen < (int)sizeof(responseBuf)) {
      if (hartSerial.available()) {
        responseBuf[respLen++] = hartSerial.read();
      }
    }
    if (respLen > 0 && hartClient && hartClient.connected()) {
      Serial.printf("HART→TCP %d bytes (data response)\n", respLen);
      sendHartIP(hartClient, HARTIP_DATA_RESP, responseBuf, respLen);
    }
  }

  // (Opcional) 4) Implementar KEEP-ALIVE periódico e DISCONNECT ao fechar
}