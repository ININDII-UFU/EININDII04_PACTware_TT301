#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "display_c.h"
#include "wifimanager_c.h"
#include "OTA.h"

#define def_pin_SCL       22    
#define def_pin_SDA       21 

WifiManager_c wm;

char DDNSName[15] = "inindkit";
Display_c disp;

// ======= Adição UDP para Teleplot =======
WiFiUDP udp;
const unsigned int teleplotPort = 47269; // Porta que você vai digitar no Teleplot

// Função para enviar dados ao Teleplot
void enviarTeleplot(String nome, float valor) {
  String mensagem = nome + ":" + String(valor) + "\n";
  udp.beginPacket("255.255.255.255", teleplotPort); // Broadcast local
  udp.write((const uint8_t*)mensagem.c_str(), mensagem.length());
  udp.endPacket();
}
// ========================================

// ========================================================
// setup() – inicialização do sistema
// ========================================================
void setup() {
    Serial.begin(115200);
    // EEPROM para ler a identificação do kit:
    EEPROM.begin(1);
    char idKit[2] = "0";
    idKit[0] = (char)EEPROM.read(0); // id do kit
    strcat(DDNSName, idKit);
    // Inicializa o Display:
    if (startDisplay(&disp, def_pin_SDA, def_pin_SCL)) {
        disp.setText(1, "Inicializando...");
    } else {
        Serial.println("Display error.");
    }
    delay(50);
    
    // Inicia conexão WiFi:
    WiFi.mode(WIFI_STA);
    // wm.start(&WSerial);
    wm.setApName(DDNSName);
    disp.setFuncMode(true);
    disp.setText(1, "Mode: Acces Point", true);
    disp.setText(2, "SSID: AutoConnectAP", true);
    disp.setText(3, "PSWD: ", true);
    if (wm.autoConnect("AutoConnectAP")) {
        disp.setFuncMode(false);
        disp.setText(1, (WiFi.localIP().toString() + " ID:" + String(idKit[0])).c_str());
        disp.setText(2, DDNSName);
        disp.setText(3, "UFU Mode");
        delay(50);

        // ======= Inicializa o UDP Teleplot após conectar WiFi =======
        udp.begin(teleplotPort); // Começa a escutar na porta UDP
        Serial.print("UDP para Teleplot iniciado na porta ");
        Serial.println(teleplotPort);
        Serial.print("Digite no Teleplot: ");
        Serial.print(WiFi.localIP());
        Serial.print(":");
        Serial.println(teleplotPort);
        // ===========================================================
    } else {
        Serial.println("Wifi error.\nAP MODE...");
    }

    // Inicia OTA e Telnet:
    OTA::start(DDNSName);  // Deve ocorrer após a conexão WiFi
}

// ========================================================
// loop() – pode conter apenas o gerenciamento de OTA, display, etc.
// ========================================================
void loop() {
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();

  // ======= Exemplo de envio de dado para Teleplot =======
  // Pode usar a função em qualquer lugar!
  static unsigned long lastTeleplot = 0;
  if (millis() - lastTeleplot > 500) {  // Envia a cada 500 ms
    float valorAleatorio = random(200, 300) / 10.0;
    enviarTeleplot("dado1", valorAleatorio);
    lastTeleplot = millis();
  }
  // ======================================================
}