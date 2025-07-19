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

// ======= CLASSE UDP: escuta 8080, envia 47269 =======
class UdpFixedPortSender {
  WiFiUDP udp;
  IPAddress destIP;
  const uint16_t destPort = 47269;   // Porta fixa de envio!
  bool ready = false;
  const unsigned int listenPort = 8080; // Porta de escuta

public:
  void begin() {
    udp.begin(listenPort);
    Serial.printf("[INFO] Escutando comandos UDP na porta %u\n", listenPort);
  }

  void handleCommand() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      IPAddress newIP = udp.remoteIP();
      char buffer[128] = {0};
      int len = udp.read(buffer, sizeof(buffer)-1);
      buffer[len] = 0;
      Serial.printf("[INFO] Pacote recebido de %s:%u - %s\n", newIP.toString().c_str(), udp.remotePort(), buffer);

      destIP = newIP;
      ready = true;
      Serial.printf("[INFO] Agora enviando para %s:%u (porta fixa)\n", destIP.toString().c_str(), destPort);
    }
  }

  void stop() {
    ready = false;
    Serial.println("[INFO] Parou de enviar dados.");
  }

  void enviarRaw(const String &msg) {
    if (!ready) {
      Serial.println("[INFO] Não há destino definido ainda.");
      return;
    }
    udp.beginPacket(destIP, destPort); // Sempre envia para porta 47269!
    udp.write((const uint8_t*)msg.c_str(), msg.length());
    udp.endPacket();
    Serial.printf("[DEBUG] Enviado para %s:%u -> %s\n", destIP.toString().c_str(), destPort, msg.c_str());
  }

  bool isReady() { return ready; }
};

UdpFixedPortSender teleplotSender;

// ========================================================
void setup() {
    Serial.begin(115200);

    EEPROM.begin(1);
    char idKit[2] = "0";
    idKit[0] = (char)EEPROM.read(0); // id do kit
    strcat(DDNSName, idKit);

    if (startDisplay(&disp, def_pin_SDA, def_pin_SCL)) {
        disp.setText(1, "Inicializando...");
    } else {
        Serial.println("Display error.");
    }
    delay(50);

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

        teleplotSender.begin();
        Serial.print("UDP Dinâmico para Teleplot iniciado na porta ");
        Serial.println(8080);
        Serial.print("Aguardando qualquer pacote UDP em: ");
        Serial.print(WiFi.localIP());
        Serial.print(":8080\n");
    } else {
        Serial.println("Wifi error.\nAP MODE...");
    }

    OTA::start(DDNSName);
}

void loop() {
    OTA::handle();
    updateDisplay(&disp);
    if (wm.getPortalRunning()) wm.process();

    teleplotSender.handleCommand();

    static unsigned long lastTeleplot = 0;
    if (teleplotSender.isReady() && millis() - lastTeleplot > 500) {
        float valorAleatorio = random(200, 300) / 10.0;
        String mensagem = "dado1:" + String(valorAleatorio, 2) + "|g";
        teleplotSender.enviarRaw(mensagem);
        lastTeleplot = millis();
    }
}