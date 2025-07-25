#include <WiFi.h>

#define HART_PREAMBULO 5
#define TCP_FRAME_TIMEOUT_MS 3000
#define HART_RESPONSE_TIMEOUT_MS 500

const uint8_t ff[HART_PREAMBULO] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const char *ssid = "APJosue";
const char *password = "josue32154538";
const IPAddress local_ip(200, 19, 148, 112);
const IPAddress gateway(200, 19, 148, 1);
const IPAddress subnet(255, 255, 255, 0);

WiFiServer server(5094);
HardwareSerial &hartSerial = Serial2;

enum HartIpState {
    WAIT_SESSION_INIT,
    SESSION_ESTABLISHED
};
HartIpState state = WAIT_SESSION_INIT;

void printHex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void send_session_response(WiFiClient &client, const uint8_t *req, size_t len) {
    uint8_t resp[32];
    memcpy(resp, req, len);
    resp[1] = 0x01;
    resp[2] = 0x00;
    client.write(resp, len);
    Serial.print("[SEND]: ");
    printHex(resp, len);
}

void send_hart_response(WiFiClient &client, const uint8_t *hart, size_t len, uint8_t seq_hi, uint8_t seq_lo) {
    uint8_t resp[8 + 256];
    resp[0] = 0x01;
    resp[1] = 0x01;
    resp[2] = 0x03;
    resp[3] = 0x00;
    resp[4] = seq_hi;
    resp[5] = seq_lo;
    resp[6] = ((8 + len) >> 8) & 0xFF;
    resp[7] = (8 + len) & 0xFF;
    memcpy(&resp[8], hart, len);
    client.write(resp, 8 + len);
    Serial.print("[SEND]: ");
    printHex(resp, 8 + len);
}

bool matchHartFrame(const uint8_t *data, size_t len) {
    const uint8_t originalFrame[] = {0x82, 0x80, 0x02, 0x00, 0x00, 0x01, 0x4A, 0x00, 0x4B};
    return len == sizeof(originalFrame) && memcmp(data, originalFrame, sizeof(originalFrame)) == 0;
}

void patchHartFrame(uint8_t *data) {
    const uint8_t correctedFrame[] = {0x82, 0x80, 0x02, 0x00, 0x00, 0x01, 0x0D, 0x00, 0x0C};
    memcpy(data, correctedFrame, sizeof(correctedFrame));
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== ESP32 HART-IP Gateway ===");

    WiFi.mode(WIFI_STA);
    WiFi.config(local_ip, gateway, subnet);
    WiFi.begin(ssid, password);
    Serial.print("Conectando ao WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado.");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
    server.begin();
    Serial.println("Aguardando conexão TCP na porta 5094...");

    hartSerial.begin(1200, SERIAL_8O1, 16, 17);
}

void loop() {
    WiFiClient client = server.available();
    if (client) {
        Serial.println("=== NOVA CONEXÃO TCP RECEBIDA ===");
        uint8_t frame[512];
        size_t frame_len = 0;
        state = WAIT_SESSION_INIT;

        while (client.connected()) {
            unsigned long t0 = millis();
            frame_len = 0;

            while (frame_len < 8 && (millis() - t0 < TCP_FRAME_TIMEOUT_MS)) {
                if (client.available()) {
                    frame[frame_len++] = client.read();
                    t0 = millis();
                }
            }
            if (frame_len < 8) break;

            size_t total_needed = (frame[6] << 8) | frame[7];
            if (total_needed < 8 || total_needed > sizeof(frame)) break;

            t0 = millis();
            while (frame_len < total_needed && (millis() - t0 < TCP_FRAME_TIMEOUT_MS)) {
                if (client.available()) {
                    frame[frame_len++] = client.read();
                    t0 = millis();
                }
            }
            if (frame_len < total_needed) break;

            Serial.print("[REC ]: ");
            printHex(frame, frame_len);

            uint8_t msg_type = frame[1];
            uint8_t msg_id = frame[2];

            if (state == WAIT_SESSION_INIT && msg_type == 0x00 && msg_id == 0x00) {
                send_session_response(client, frame, total_needed);
                state = SESSION_ESTABLISHED;
            } else if (state == SESSION_ESTABLISHED && msg_type == 0x00 && msg_id == 0x03 && total_needed > 8) {
                uint8_t *hart_payload = &frame[8];
                size_t hart_len = total_needed - 8;

                // Corrigir frame problemático
                if (matchHartFrame(hart_payload, hart_len)) {
                    Serial.println("[INFO] Frame HART-IP problemático detectado. Corrigindo para 0D 00 0C...");
                    patchHartFrame(hart_payload);
                }

                hartSerial.write(ff, HART_PREAMBULO);
                hartSerial.write(hart_payload, hart_len);

                unsigned long uart_t0 = millis();
                uint8_t hart_resp[256];
                size_t resp_len = 0;
                while ((millis() - uart_t0 < HART_RESPONSE_TIMEOUT_MS)) {
                    while (hartSerial.available() && resp_len < sizeof(hart_resp)) {
                        hart_resp[resp_len++] = hartSerial.read();
                    }
                }

                size_t preamb = 0;
                while (preamb < resp_len && hart_resp[preamb] == 0xFF) preamb++;

                if (resp_len > preamb) {
                    send_hart_response(client, hart_resp + preamb, resp_len - preamb, frame[4], frame[5]);
                }
            } else if (msg_type == 0x00 && msg_id == 0x01) {
                client.stop();
                break;
            } else {
                Serial.println("[WARN] Comando desconhecido ignorado.");
            }
        }
        client.stop();
    }
    delay(10);
}