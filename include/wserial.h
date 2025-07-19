#ifndef __WSERIAL_H
#define __WSERIAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

/**
 * @brief Comunicação Serial+UDP, compatível com Teleplot-style para ESP32.
 */
class WSerial {
protected:
  WiFiUDP udp;                         ///< Instância UDP.
  IPAddress destIP;                    ///< IP do último remetente UDP.
  uint16_t destPort = 0;               ///< Porta do último remetente UDP.
  uint16_t listenPort = 0;             ///< Porta local de escuta.
  bool ready = false;                  ///< Pronto para envio UDP?
  HardwareSerial* hwSerial = &Serial;  ///< Serial usada (default: Serial).

public:
  /**
   * @brief Inicializa UDP e define Serial.
   * @param serial Serial desejada (default: Serial).
   * @param port Porta UDP de escuta (default: 8080).
   */
  void begin(HardwareSerial* serial = &Serial, uint16_t port = 8080) {
    listenPort = port;
    hwSerial = serial;
    udp.begin(listenPort);
  }

  /**
   * @brief Atualiza estado do UDP (deve ser chamado em loop).
   */
  void update() {
    if (WiFi.status() != WL_CONNECTED) { ready = false; return; }
    int packetSize = udp.parsePacket();
    if (packetSize) {
      destIP = udp.remoteIP();
      destPort = udp.remotePort();
      char buffer[128] = {0}; // Já zera todo o buffer!
      int len = udp.read(buffer, sizeof(buffer) - 1);
      if (len > 0) {
        buffer[len] = 0; // Garantia do null-terminator
        ready = true;
      } else {
        ready = false;
      }
    }
  }


  /**
   * @brief Para envio UDP.
   */
  void stop() { ready = false; }

  /**
   * @brief Envia mensagem bruta via UDP ou Serial.
   */
  void sendRaw(const String &msg) {
    if (ready && WiFi.status() == WL_CONNECTED) {
      udp.beginPacket(destIP, destPort);
      udp.write((const uint8_t*)msg.c_str(), msg.length());
      udp.endPacket();
    } else {
      hwSerial->print(msg);
    }
  }

  /// Checa se está pronto para UDP
  bool isReady() const { return ready && WiFi.status() == WL_CONNECTED; }
  /// Porta de destino UDP
  uint16_t getDestPort() const { return destPort; }
  /// Porta de escuta UDP
  uint16_t getListenPort() const { return listenPort; }

  // ===== Métodos compatíveis Serial/UDP =====
  template <typename T>
  void print(const T &data)          { sendRaw(String(data)); }
  template <typename T>
  void print(const T &data, int base){ sendRaw(String(data, base)); }
  template <typename T>
  void println(const T &data)        { sendRaw(String(data) + "\n"); }
  template <typename T>
  void println(const T &data, int base) { sendRaw(String(data, base) + "\n"); }
  void println()                     { sendRaw("\n"); }

  // ===== Estilo Teleplot =====
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
};

#endif