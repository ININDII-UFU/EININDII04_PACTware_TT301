#ifndef __WSERIAL_H
#define __WSERIAL_H

#include <Arduino.h>
#include <ESPTelnet.h>

#define BAUD_RATE 115200

class WSerial : public ESPTelnet {
public:
  WSerial() = default;

  bool start(uint16_t port);
  void update();
  bool isConnected() { return ESPTelnet::isConnected(); }
  uint16_t serverPort() const { return server_port; }

  template <typename TX, typename TY>
  void plot(const char* varName, TX x, TY y, const char* unit = nullptr) {
    this->print(">");
    this->print(varName);
    this->print(":");
    this->print(x);
    this->print(":");
    this->print(y);
    if (unit) {
        this->print("§");
        this->print(unit);
    }
    this->println("|g");
  }


  template <typename T>
  void plot(const char* varName, T y, const char* unit = nullptr) {
    plot(varName, millis(), y, unit);
  }

  template <typename T> void print(const T& data) {
    if (isConnected()) ESPTelnet::print(data);
    else Serial.print(data);
  }

  template <typename T> void print(const T& data, int base) {
    if (isConnected()) ESPTelnet::print(data, base);
    else Serial.print(data, base);
  }

  void println() {
    if (isConnected()) ESPTelnet::println();
    else Serial.println();
  }

  template <typename T> void println(const T& data) {
    if (isConnected()) ESPTelnet::println(data);
    else Serial.println(data);
  }

  template <typename T> void println(const T& data, int base) {
    if (isConnected()) ESPTelnet::println(data, base);
    else Serial.println(data, base);
  }

  friend inline bool startWSerial(WSerial* ws, uint16_t port) {
    return ws->start(port);
  }

  friend inline void updateWSerial(WSerial* ws) {
    ws->update();
  }

private:
  uint16_t server_port = 0;
};

inline bool WSerial::start(uint16_t port) {
  server_port = port;

  onDisconnect([](String ip) {
    Serial.printf("- Telnet: %s disconnected\n", ip.c_str());
  });

  onConnectionAttempt([](String ip) {
    Serial.printf("- Telnet: %s tried to connect\n", ip.c_str());
  });

  onReconnect([](String ip) {
    Serial.printf("- Telnet: %s reconnected\n", ip.c_str());
  });

  return begin(server_port);
}

inline void WSerial::update() {
  if (isConnected() && Serial.available() && on_input) {
    on_input(Serial.readStringUntil('\n'));
  }
  loop();
}

#endif