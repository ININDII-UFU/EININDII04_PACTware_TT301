#ifndef __WSERIAL_H
#define __WSERIAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ======= CLASSE UDP: escuta porta dinâmica, envia para porta de origem recebida =======
class UdpFixedPortSender {
protected:
  WiFiUDP udp;
  IPAddress destIP;
  uint16_t destPort = 0;            // Porta de envio (capturada no handleCommand)
  bool ready = false;
  unsigned int listenPort = 0;      // Porta de escuta (setada pelo método begin/dinamicamente)

public:
  void begin(unsigned int port) {
    listenPort = port;
    udp.begin(listenPort);
    Serial.printf("[INFO] Escutando comandos UDP na porta %u\n", listenPort);
  }

  void handleCommand() {
    if (WiFi.status() != WL_CONNECTED) {
      ready = false;
      return;
    }
    int packetSize = udp.parsePacket();
    if (packetSize) {
      IPAddress newIP = udp.remoteIP();
      uint16_t newPort = udp.remotePort();      // <- pega a porta de origem!
      char buffer[128] = {0};
      int len = udp.read(buffer, sizeof(buffer)-1);
      buffer[len] = 0;
      Serial.printf("[INFO] Pacote recebido de %s:%u - %s\n", newIP.toString().c_str(), newPort, buffer);

      destIP = newIP;
      destPort = newPort;
      ready = true;
      Serial.printf("[INFO] Agora enviando para %s:%u (porta recebida)\n", destIP.toString().c_str(), destPort);
    }
  }

  void stop() {
    ready = false;
    Serial.println("[INFO] Parou de enviar dados.");
  }

  void enviarRaw(const String &msg) {
    // Só envia UDP se WiFi está conectado e destino está definido
    if (!ready || WiFi.status() != WL_CONNECTED) {
      Serial.println("[INFO] Não há destino UDP definido ou WiFi não conectado. Envio somente via Serial.");
      return;
    }
    udp.beginPacket(destIP, destPort); // Envia para IP:porta capturados!
    udp.write((const uint8_t*)msg.c_str(), msg.length());
    udp.endPacket();
    Serial.printf("[DEBUG] Enviado para %s:%u -> %s\n", destIP.toString().c_str(), destPort, msg.c_str());
  }

  bool isReady() { 
    // Só está "pronto" se WiFi está conectado E destino já definido
    return ready && WiFi.status() == WL_CONNECTED; 
  }
  uint16_t getDestPort() const { return destPort; }
  unsigned int getListenPort() const { return listenPort; }
};

// ===============================================
class WSerial_c : public UdpFixedPortSender
{
public:
  WSerial_c() : UdpFixedPortSender() {}
  void update(void);
  bool begin(unsigned long baudrate = 115200 ,uint16_t port = 8080);
  // plot - para gráfico (modo Teleplot)
  template <typename T>
  void plot(const char *varName, T x, T y, const char *unit = NULL);
  template <typename T>
  void plot(const char *varName, T y, const char *unit = NULL);

  // print/println
  template <typename T>
  void print(const T &data);
  template <typename T>
  void print(const T &data, int base);
  void println();
  template <typename T>
  void println(const T &data);
  template <typename T>
  void println(const T &data, int base);

  uint16_t serverPort() { return this->getListenPort(); }     // retorna porta atual de escuta
  bool isConnected();
};


bool WSerial_c::begin(unsigned long baudrate,uint16_t listenPort)
{
  Serial.begin(baudrate);
  UdpFixedPortSender::begin(listenPort);   // Chama begin() da base, que seta listenPort e inicia UDP
  return true;
}

inline bool WSerial_c::isConnected()
{
  return UdpFixedPortSender::isReady();
}

void WSerial_c::update(void)
{
  UdpFixedPortSender::handleCommand();
}

// ==== plot, print, println implementações ====

template <typename T>
void WSerial_c::plot(const char *varName, T y, const char *unit)
{
  plot(varName, (T)millis(), y, unit);
}

template <typename T>
void WSerial_c::plot(const char *varName, T x, T y, const char *unit)
{
  String msg = ">";
  msg += varName;
  msg += ":";
  msg += String(x);
  msg += ":";
  msg += String(y);
  if (unit != NULL)
  {
    msg += "§";
    msg += unit;
  }
  msg += "|g";
  println(msg);
}

template <typename T>
void WSerial_c::print(const T &data)
{
  if (isConnected()) {
    String msg = String(data);    
    UdpFixedPortSender::enviarRaw(msg);  
  } else Serial.print(data);  
}

template <typename T>
void WSerial_c::print(const T &data, int base)
{
  if (isConnected()) {
    String msg = String(data, base);    
    UdpFixedPortSender::enviarRaw(msg);  
  } else Serial.print(data, base);  
}

template <typename T>
void WSerial_c::println(const T &data)
{
  if (isConnected()) {
    String msg = String(data) + "\n";
    UdpFixedPortSender::enviarRaw(msg);  
  } else Serial.println(data); 
}

template <typename T>
void WSerial_c::println(const T &data, int base)
{
  if (isConnected()) {
    String msg = String(data, base) + "\n";
    UdpFixedPortSender::enviarRaw(msg);  
  } else Serial.println(data, base);  
}

void WSerial_c::println()
{
  if (isConnected()) UdpFixedPortSender::enviarRaw("\n");  
  else Serial.println();
}

#endif