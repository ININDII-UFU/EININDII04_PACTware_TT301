#ifndef __WSERIAL_H
#define __WSERIAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

/**
 * @brief Comunicação Serial+UDP Broadcast (Teleplot-style) para ESP32.
 */
class WSerial {
protected:
  WiFiUDP udp;                         ///< Instância UDP.
  IPAddress broadcastIP;               ///< IP de broadcast.
  uint16_t destPort = 47269;           ///< Porta UDP de destino (padrão Teleplot).
  HardwareSerial* hwSerial = &Serial;  ///< Serial usada.

public:
  /**
   * @brief Inicializa o UDP Broadcast e define Serial.
   * @param serial Serial desejada (default: Serial).
   * @param port Porta UDP destino (default: 47269).
   */
  void begin(HardwareSerial* serial = &Serial, uint16_t port = 47269) {
    destPort = port;
    hwSerial = serial;
    // NÃO chame udp.begin() se só for enviar!
    IPAddress ip = WiFi.localIP();
    IPAddress subnet = WiFi.subnetMask();
    broadcastIP = IPAddress(
      ip[0] | ~subnet[0],
      ip[1] | ~subnet[1],
      ip[2] | ~subnet[2],
      ip[3] | ~subnet[3]
    );
    if (hwSerial) {
      hwSerial->printf("[INFO] Enviando UDP broadcast para %s:%u\n", broadcastIP.toString().c_str(), destPort);
    }
  }

  /**
   * @brief Envia mensagem bruta via UDP broadcast.
   */
  void sendRaw(const String &msg) {
    if (WiFi.status() == WL_CONNECTED) {
      udp.beginPacket(broadcastIP, destPort);
      udp.write((const uint8_t*)msg.c_str(), msg.length());
      udp.endPacket();
      if (hwSerial) hwSerial->printf("[DEBUG] Pacote enviado: %s\n", msg.c_str());
    } else if (hwSerial) {
      hwSerial->print(msg);
    }
  }

  // Métodos compatíveis Serial/UDP
  template <typename T>
  void print(const T &data)          { sendRaw(String(data)); }
  template <typename T>
  void print(const T &data, int base){ sendRaw(String(data, base)); }
  template <typename T>
  void println(const T &data)        { sendRaw(String(data) + "\n"); }
  template <typename T>
  void println(const T &data, int base) { sendRaw(String(data, base) + "\n"); }
  void println()                     { sendRaw(String("\n")); }

  // Estilo Teleplot
  template <typename T>
  void plot(const char *var, T y, const char *unit = NULL) {
    plot(var, (T)millis(), y, unit);
  }
  template <typename T>
  void plot(const char *var, T x, T y, const char *unit = NULL) {
    String msg = ">";
    msg += var; msg += ":"; msg += String(x); msg += ":"; msg += String(y);
    if (unit) { msg += "§"; msg += unit; }
    msg += "|g";
    println(msg);
  }
  // Função para enviar dados ao Teleplot
  void enviarTeleplot(String nome, float valor) {
    String mensagem = nome + ":" + String(valor) + "\n";
    udp.beginPacket("255.255.255.255", destPort); // Envia broadcast na rede local
    udp.write((const uint8_t*)mensagem.c_str(), mensagem.length());
    udp.endPacket();
  }
};

#endif