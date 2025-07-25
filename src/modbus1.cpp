// coil status:       [0] def_pin_D2,   [1] def_pin_D3,   [2] def_pin_D4,             [3] def_pin_RELE,           [4]             [5]
// Input status:      [0] def_pin_RTN1, [1] def_pin_RTN2, [2] def_pin_PUSH1,          [3] def_pin_PUSH2,          [4] def_pin_D1, [5]
// Input Registers:   [0] POT1,         [1] POT2,         [2] Leitura 4-20mA canal 1, [3] Leitura 4-20mA canal 2, [4] ADC1,       [5] ADC2,
// Holding Registers: [0] DAC,          [1] Write 4-20mA, [2] PWM                     [3]                         [4]             [5]

#include <Arduino.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
#include "OTA.h"
#include "display_c.h"
#include "wifimanager_c.h"
#include "ads1115_c.h"

/********** Definições dos GPIO ***********/
#define def_pin_ADC1 36
#define def_pin_ADC2 39
#define def_pin_RTN2 35
#define def_pin_PUSH1 34
#define def_pin_PWM 33
#define def_pin_PUSH2 32
#define def_pin_RELE 27
#define def_pin_W4a20_1 26
#define def_pin_DAC1 25
#define def_pin_D1 23
#define def_pin_SCL 22
#define def_pin_SDA 21
#define def_pin_D2 19
#define def_pin_D3 18
#define def_pin_D4 4
#define def_pin_RTN1 2

#define CHANNEL_W4a20 0
#define CHANNEL_PWM 1

// ModbusIP instance
ModbusIP mb;

// Variáveis e objetos do projeto:
char DDNSName[15] = "inindkit";
ADS1115_c ads;
WifiManager_c wm;
Display_c disp;

#define COILSIZE 4
#define STATUSSIZE 5
#define HRSIZE 3
#define IRSIZE 6

volatile uint16_t holdingRegisters[HRSIZE] = {50, 30, 40};  //[0]: DAC, [1]: Wriete 4-20mA, [2]: PWM
volatile bool coils[COILSIZE] = {false, false, false, false}; //[0]: D2, [1]: D3, [2]: D4, [3]: RELÊ

HardwareSerial &hartSerial = Serial2;

void setup() {
  Serial.begin(1200, SERIAL_8O1);             // Serial USB (PACTware) — 1200 8O1
  hartSerial.begin(1200, SERIAL_8O1, 16, 17); // UART2 (Modem HART) — 1200 8O1, TX=17, RX=16

  // EEPROM e DDNSName
  EEPROM.begin(1);
  char idKit[2] = "0";
  // EEPROM.write(0, (uint8_t)idKit[0]);
  // EEPROM.commit();
  idKit[0] = (char)EEPROM.read(0); // id do kit
  strcat(DDNSName, String(idKit).c_str());
  // Inicializa o Display:
  if (startDisplay(&disp, def_pin_SDA, def_pin_SCL)) disp.setText(1, "Inicializando...");

  // Inicia conexão WiFi:
  WiFi.mode(WIFI_STA);
  // wm.start(&WSerial);
  wm.setApName(DDNSName);
  disp.setFuncMode(true);
  disp.setText(1, "Mode: Acces Point", true);
  disp.setText(2, "SSID: AutoConnectAP", true);
  disp.setText(3, "PSWD: ", true);
  if (wm.autoConnect("AutoConnectAP"))
  {
    disp.setFuncMode(false);
    disp.setText(1, (WiFi.localIP().toString() + " ID:" + String(idKit[0])).c_str());
    disp.setText(2, DDNSName);
    disp.setText(3, "UFU Mode");
    delay(50);
  }

  // OTA init
  OTA::start(DDNSName);

  // Pinos
  pinMode(def_pin_ADC1, INPUT);
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

  // PWM channels
  ledcSetup(CHANNEL_W4a20, 19000, 12);
  ledcAttachPin(def_pin_W4a20_1, CHANNEL_W4a20);
  ledcWrite(CHANNEL_W4a20, 0);
  ledcSetup(CHANNEL_PWM, 19000, 12);
  ledcAttachPin(def_pin_PWM, CHANNEL_PWM);
  ledcWrite(CHANNEL_PWM, 0);

  dacWrite(def_pin_DAC1, 0);
  ads.begin();

  // Modbus coil and register mapping
  for (uint16_t i = 0; i < COILSIZE; i++) {
    mb.addCoil(i, coils[i]);
  }
  for (uint16_t i = 0; i < HRSIZE; i++) {
    mb.addHreg(i, holdingRegisters[i]);
  }

  // Start MDNS
  if (MDNS.begin(DDNSName)) {
    MDNS.addService("modbus", "tcp", 502);
  }
}

void loop() {
  // OTA, Display, Serial
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();
  // Handle Modbus/IP
  mb.task();

  // PACTware → HART Modem
  while (Serial.available()>0) hartSerial.write(Serial.read());
  // HART Modem → PACTware
  while (hartSerial.available()>0) Serial.write(hartSerial.read());

  // Check for coil writes and apply outputs
  for (uint16_t i = 0; i < COILSIZE; i++) {
    bool state = mb.Coil(i);
    if (state != coils[i]) {
      coils[i] = state;
      int pin = (i == 0 ? def_pin_D2 : i == 1 ? def_pin_D3 : i == 2 ? def_pin_D4 : def_pin_RELE);
      digitalWrite(pin, state ? HIGH : LOW);
    }
  }

  // Update Holding Registers outputs
  for (uint16_t i = 0; i < HRSIZE; i++) {
    uint16_t val = mb.Hreg(i);
    if (val != holdingRegisters[i]) {
      holdingRegisters[i] = val;
      uint8_t out;
      switch (i) {
        case 0:
          out = map(val, 0, 65535, 0, 255);
          dacWrite(def_pin_DAC1, out);
          break;
        case 1:
          out = map(val, 0, 65535, 0, 4096);
          ledcWrite(CHANNEL_W4a20, out);
          break;
        case 2:
          out = map(val, 0, 65535, 0, 4096);
          ledcWrite(CHANNEL_PWM, out);
          break;
      }
    }
  }

  // Update Input Registers (read sensors)
  mb.Hreg(0, ads.analogRead(1));
  mb.Hreg(1, ads.analogRead(0));
  uint16_t vlTRM1 = ads.analogRead(3);
  mb.Hreg(2, vlTRM1);
  disp.setText(2, ("TRM1:" + String(vlTRM1)).c_str());  
  uint16_t vlTRM2 = ads.analogRead(2);  
  mb.Hreg(3, vlTRM2);
  disp.setText(3, ("TRM2:" + String(vlTRM2)).c_str());  
  mb.Hreg(4, map(analogRead(def_pin_ADC1), 0, 4096,0, 65535));  
  mb.Hreg(5, map(analogRead(def_pin_ADC2), 0, 4096,0, 65535));
}