#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "display_c.h"
#include "wifimanager_c.h"
#include "wserial.h"
#include "ads1115_c.h"
#include "jtask.h"
#include "OTA.h"

#define listenPort 8080UL
#define baudrate 115200
#define def_pin_SCL       22    
#define def_pin_SDA       21 

ADS1115_c ads;
WifiManager_c wm;
WSerial wserial;
char DDNSName[15] = "inindkit";
Display_c disp;

//Função que le os valores dos POT e das Entradas 4 a 20 mA e plota no display
void managerInputFunc(void) {
    const uint16_t vlPOT1 = ads.analogRead(1);
    const uint16_t vlPOT2 = ads.analogRead(0);
    disp.setText(2, ("P1:" + String(vlPOT1)).c_str());
    disp.setText(3, ("P2:" + String(vlPOT2)).c_str());    
    wserial.plot("vlPOT1", vlPOT1);
    wserial.plot("vlPOT2", vlPOT2);
}


// ========================================================
void setup() {
    Serial.begin(baudrate);

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
    } else {
        Serial.println("Wifi error.\nAP MODE...");
    }
    OTA::start(DDNSName);
    wserial.begin(&Serial, listenPort);    
    jtaskAttachFunc(managerInputFunc, 50000UL); //anexa um função e sua base de tempo para ser executada
}

void loop() {
    OTA::handle();
    updateDisplay(&disp);
    if (wm.getPortalRunning()) wm.process();
    wserial.update();
    if(wserial.isReady()) jtaskLoop();
}