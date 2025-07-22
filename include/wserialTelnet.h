#ifndef __WSERIAL_H
#define __WSERIAL_H

#include <Arduino.h>
#include <ESPTelnet.h>

#define BAUD_RATE 115200

class WSerial : public ESPTelnet {
public:
  WSerial() = default;

  bool start(uint16_t port, unsigned long baudrate = BAUD_RATE);
  void update();
  bool isConnected() { return ESPTelnet::isConnected(); }
  uint16_t serverPort() const { return server_port; }

  template <typename T>
  void plot(const char* varName, T y, const char* unit = nullptr) {
    plot(varName, millis(), y, unit);
  }

  template <typename T>
  void plot(const char* varName, T x, T y, const char* unit = nullptr) {
    print(">");
    print(varName); print(":"); print(x); print(":"); print(y);
    if (unit) { print("§"); print(unit); }
    println("|g");
  }

  template <typename T> void print(const T& data) {
    isConnected() ? ESPTelnet::print(data) : Serial.print(data);
  }

  template <typename T> void print(const T& data, int base) {
    isConnected() ? ESPTelnet::print(data, base) : Serial.print(data, base);
  }

  void println() {
    isConnected() ? ESPTelnet::println() : Serial.println();
  }

  template <typename T> void println(const T& data) {
    isConnected() ? ESPTelnet::println(data) : Serial.println(data);
  }

  template <typename T> void println(const T& data, int base) {
    isConnected() ? ESPTelnet::println(data, base) : Serial.println(data, base);
  }

  friend inline bool startWSerial(WSerial_c* ws, uint16_t port, unsigned long baudrate = BAUD_RATE) {
    return ws->start(port, baudrate);
  }

  friend inline void updateWSerial(WSerial_c* ws) {
    ws->update();
  }

private:
  uint16_t server_port = 0;
};

inline bool WSerial::start(uint16_t port,  unsigned long baudrate) {
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
  ((ESPTelnet *) this)->loop();
}

#endif