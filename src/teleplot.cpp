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

// ======= UDP para Teleplot como servidor =======
WiFiUDP udp;
const unsigned int teleplotPort = 47269; // Porta para Teleplot conectar

IPAddress teleplotClientIP;    // IP de quem conectou (Teleplot)
unsigned int teleplotClientPort = 0; // Porta de quem conectou (Teleplot)
bool teleplotConnected = false;

// Função para enviar dados ao Teleplot no formato oficial
void enviarTeleplot(String nome, float valor) {
  if (!teleplotConnected) return;

  // Formato Teleplot: nome:valor|g
  String mensagem = nome + ":" + String(valor, 2) + "|g";
  udp.beginPacket(teleplotClientIP, teleplotClientPort);
  udp.write((const uint8_t*)mensagem.c_str(), mensagem.length());
  udp.endPacket();
}

// (Opcional) Se quiser enviar com timestamp, use esta função ao invés da de cima
void enviarTeleplotComTimestamp(String nome, float valor) {
  if (!teleplotConnected) return;

  // Formato Teleplot com timestamp: nome:timestamp:valor|g
  String mensagem = nome + ":" + String(millis()) + ":" + String(valor, 2) + "|g";
  udp.beginPacket(teleplotClientIP, teleplotClientPort);
  udp.write((const uint8_t*)mensagem.c_str(), mensagem.length());
  udp.endPacket();
}
// ===============================================

// ========================================================
// setup() – inicialização do sistema
// ========================================================
void setup() {
    Serial.begin(115200);
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
        Serial.print("Servidor UDP para Teleplot iniciado na porta ");
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
// loop()
// ========================================================
void loop() {
  OTA::handle();
  updateDisplay(&disp);
  if (wm.getPortalRunning()) wm.process();

  // Verifica se recebeu "hello" do Teleplot
  int packetSize = udp.parsePacket();
  if (packetSize) {
    teleplotClientIP = udp.remoteIP();
    teleplotClientPort = udp.remotePort();
    teleplotConnected = true;
    Serial.print("Teleplot conectado de: ");
    Serial.print(teleplotClientIP);
    Serial.print(":");
    Serial.println(teleplotClientPort);
    // Descarta conteúdo recebido (não usado)
    while (udp.available()) udp.read();
  }

  // Envia dados só se houver cliente conectado
  static unsigned long lastTeleplot = 0;
  if (teleplotConnected && (millis() - lastTeleplot > 1000)) {  // Envia a cada 1s (ajuste se quiser)
    float valorAleatorio = random(200, 300) / 10.0;
    enviarTeleplot("dado1", valorAleatorio);  // Use esta linha para enviar SEM timestamp
    // enviarTeleplotComTimestamp("dado1", valorAleatorio); // Ou esta para enviar COM timestamp
    lastTeleplot = millis();
  }
}