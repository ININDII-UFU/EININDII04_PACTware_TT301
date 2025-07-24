#include <WiFi.h>

const char *ssid = "APJosue";
const char *password = "josue32154538";
const IPAddress local_ip(200, 19, 148, 112);
const IPAddress gateway(200, 19, 148, 1);
const IPAddress subnet(255, 255, 255, 0);

WiFiServer server(5094);
HardwareSerial &hartSerial = Serial2;

#define HART_PREAMBULO 5

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
    uint8_t resp[13];
    memcpy(resp, req, len);
    resp[1] = 0x01; // Type: Response
    client.write(resp, len);
    Serial.print("[SEND] Session Initiate Response: ");
    printHex(resp, len);
}

void send_hart_response(WiFiClient &client, const uint8_t *hart, size_t len, uint8_t seq_hi, uint8_t seq_lo) {
    uint8_t resp[8 + 256];
    resp[0] = 0x01;      // Version
    resp[1] = 0x01;      // Type: Response
    resp[2] = 0x03;      // Message ID: Pass-Through Response
    resp[3] = 0x00;      // Status
    resp[4] = seq_hi;    // Sequence High
    resp[5] = seq_lo;    // Sequence Low
    resp[6] = (len >> 8) & 0xFF;
    resp[7] = len & 0xFF;
    memcpy(&resp[8], hart, len);
    client.write(resp, 8 + len);
    Serial.print("[SEND] Pass-Through Response: ");
    printHex(resp, 8 + len);
}

// Buffer TCP global e estado
WiFiClient client;
uint8_t tcpbuf[512];
size_t tcpbuf_len = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println();
    Serial.println("=== ESP32 HART-IP Gateway ===");

    WiFi.mode(WIFI_STA);
    WiFi.config(local_ip, gateway, subnet);
    WiFi.begin(ssid, password);
    Serial.print("Conectando ao WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado.");
    Serial.print("Endereço IP: "); Serial.println(WiFi.localIP());
    server.begin();
    Serial.println("Aguardando conexão TCP na porta 5094...");

    hartSerial.begin(1200, SERIAL_8O1, 16, 17);
}

void loop() {
    // Aceita nova conexão se cliente não está ativo
    if (!client || !client.connected()) {
        client = server.available();
        if (client) {
            Serial.println("=== NOVA CONEXÃO TCP RECEBIDA ===");
            state = WAIT_SESSION_INIT;
            tcpbuf_len = 0;
        }
        delay(10);
        return;
    }

    // Bufferização TCP robusta: lê tudo disponível
    while (client.available() && tcpbuf_len < sizeof(tcpbuf)) {
        tcpbuf[tcpbuf_len++] = client.read();
    }

    // Só processa frame quando cabeçalho HART-IP completo
    while (tcpbuf_len >= 8) {
        uint8_t msg_id = tcpbuf[2];
        uint16_t msg_len = (tcpbuf[6] << 8) | tcpbuf[7];
        size_t pkt_len = 8 + msg_len;

        if (tcpbuf_len < pkt_len) break; // Aguarda pacote completo

        Serial.printf("[TCP] %u bytes no buffer. MessageID=0x%02X, len=%u (pkt_len=%u)\n", (unsigned)tcpbuf_len, msg_id, msg_len, pkt_len);
        printHex(tcpbuf, pkt_len);

        if (state == WAIT_SESSION_INIT && msg_id == 0x00 && pkt_len >= 13) {
            send_session_response(client, tcpbuf, 13);
            state = SESSION_ESTABLISHED;
        }
        else if (state == SESSION_ESTABLISHED && msg_id == 0x03 && pkt_len >= 8 + msg_len) {
            uint8_t *hart_frame = &tcpbuf[8];
            size_t frame_len = msg_len;

            // Preâmbulo HART
            size_t preamb_count = 0;
            while (preamb_count < frame_len && hart_frame[preamb_count] == 0xFF)
                preamb_count++;
            if (preamb_count < 5) {
                uint8_t ff[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                hartSerial.write(ff, 5 - preamb_count);
            }
            hartSerial.write(hart_frame, frame_len);

            Serial.print("[UART] >> ");
            printHex(hart_frame, frame_len);

            delay(250);
            uint8_t hart_resp[256];
            size_t resp_len = hartSerial.readBytes(hart_resp, sizeof(hart_resp));
            if (resp_len > 0) {
                send_hart_response(client, hart_resp, resp_len, tcpbuf[4], tcpbuf[5]);
                Serial.print("[UART] << ");
                printHex(hart_resp, resp_len);
            } else {
                Serial.println("[WARN] Nenhuma resposta do modem HART");
            }
        }
        else {
            Serial.printf("[ERROR] Frame não suportado (msg_id=0x%02X, pkt_len=%u)\n", msg_id, (unsigned)pkt_len);
        }

        // Remove pacote processado do buffer
        memmove(tcpbuf, tcpbuf + pkt_len, tcpbuf_len - pkt_len);
        tcpbuf_len -= pkt_len;
    }
    delay(1);
}
