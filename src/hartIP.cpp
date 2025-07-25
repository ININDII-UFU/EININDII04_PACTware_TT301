#include <WiFi.h>

const char *ssid = "APJosue";
const char *password = "josue32154538";
const IPAddress local_ip(200, 19, 148, 112);
const IPAddress gateway(200, 19, 148, 1);
const IPAddress subnet(255, 255, 255, 0);

WiFiServer server(5094);
HardwareSerial &hartSerial = Serial2;

#define HART_PREAMBULO 5
#define TCP_FRAME_TIMEOUT_MS 3000

enum HartIpState
{
    WAIT_SESSION_INIT,
    SESSION_ESTABLISHED
};
HartIpState state = WAIT_SESSION_INIT;

void printHex(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] < 0x10)
            Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

// Resposta ao Session Initiate
void send_session_response(WiFiClient &client, const uint8_t *req, size_t len)
{
    uint8_t resp[32];
    memcpy(resp, req, len);
    resp[1] = 0x01; // Type: Response
    resp[2] = 0x00; // ID: Session Initiate
    client.write(resp, len);
    Serial.print("[SEND] Session Initiate Response: ");
    printHex(resp, len);
}

// Resposta ao Pass-Through
void send_hart_response(WiFiClient &client, const uint8_t *hart, size_t len, uint8_t seq_hi, uint8_t seq_lo)
{
    uint8_t resp[8 + 256];
    resp[0] = 0x01;                    // Version
    resp[1] = 0x01;                    // Type: Response
    resp[2] = 0x03;                    // ID: Pass-Through
    resp[3] = 0x00;                    // Status
    resp[4] = seq_hi;                  // Sequence High
    resp[5] = seq_lo;                  // Sequence Low
    resp[6] = ((8 + len) >> 8) & 0xFF; // MsgLen (header + body)
    resp[7] = (8 + len) & 0xFF;
    memcpy(&resp[8], hart, len);
    client.write(resp, 8 + len);
    Serial.print("[SEND] Pass-Through Response: ");
    printHex(resp, 8 + len);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== ESP32 HART-IP Gateway ===");

    WiFi.mode(WIFI_STA);
    WiFi.config(local_ip, gateway, subnet);
    WiFi.begin(ssid, password);
    Serial.print("Conectando ao WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
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

void loop()
{
    WiFiClient client = server.available();
    if (client)
    {
        Serial.println("=== NOVA CONEXÃO TCP RECEBIDA ===");
        uint8_t frame[512];
        size_t frame_len = 0;
        state = WAIT_SESSION_INIT;

        while (client.connected())
        {
            unsigned long t0 = millis();
            // 1. Lê exatamente 8 bytes do header
            while (frame_len < 8 && (millis() - t0 < TCP_FRAME_TIMEOUT_MS))
            {
                if (client.available())
                {
                    frame[frame_len++] = client.read();
                    t0 = millis();
                }
            }
            if (frame_len < 8)
            {
                if (frame_len > 0)
                    Serial.printf("[TIMEOUT] Recebeu só %d/8 bytes de header\n", (int)frame_len);
                break;
            }

            // 2. Pega tamanho total do frame (msg_len, header + body, sempre)
            size_t total_needed = (frame[6] << 8) | frame[7];
            if (total_needed < 8 || total_needed > sizeof(frame))
            {
                Serial.printf("[ERRO] Tamanho inválido do frame: %u bytes\n", (unsigned)total_needed);
                break;
            }

            // 3. Lê até juntar total_needed bytes no frame
            t0 = millis();
            while (frame_len < total_needed && (millis() - t0 < TCP_FRAME_TIMEOUT_MS))
            {
                if (client.available())
                {
                    frame[frame_len++] = client.read();
                    t0 = millis();
                }
            }
            if (frame_len < total_needed)
            {
                Serial.printf("[TIMEOUT] Recebeu só %d/%u bytes (header+body)\n", (int)frame_len, (unsigned)total_needed);
                break;
            }

            // 4. Processa o pacote completo HART-IP
            Serial.printf("[TCP] Frame completo recebido: %u bytes\nHEX:   ", (unsigned)frame_len);
            printHex(frame, frame_len);

            uint8_t msg_type = frame[1];
            uint8_t msg_id = frame[2];

            if (state == WAIT_SESSION_INIT && msg_type == 0x00 && msg_id == 0x00)
            {
                send_session_response(client, frame, total_needed);
                state = SESSION_ESTABLISHED;
            }
            // Pass-Through Request: Type 0x00 (Request), ID 0x03 (Pass-Through)
            else if (state == SESSION_ESTABLISHED && msg_type == 0x00 && msg_id == 0x03 && total_needed > 8)
            {
                size_t hart_len = total_needed - 8;
                uint8_t *hart_cmd = &frame[8];

                // Sempre envia 5 bytes de preâmbulo, independentemente do frame recebido
                uint8_t ff[HART_PREAMBULO] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                hartSerial.write(ff, HART_PREAMBULO);
                // Envia o resto do comando, pulando os 0xFF iniciais se vierem a mais
                size_t cmd_start = 0;
                while (cmd_start < hart_len && hart_cmd[cmd_start] == 0xFF)
                    cmd_start++;
                hartSerial.write(hart_cmd + cmd_start, hart_len - cmd_start);
                Serial.print("[UART] >> ");
                printHex(hart_cmd, hart_len);

                // Leitura da resposta do modem HART
                unsigned long uart_t0 = millis();
                size_t resp_len = 0;
                uint8_t hart_resp[256];
                while ((millis() - uart_t0 < 250))
                {
                    while (hartSerial.available() && resp_len < sizeof(hart_resp))
                    {
                        hart_resp[resp_len++] = hartSerial.read();
                    }
                }
                if (resp_len > 0)
                {
                    size_t off = 0;
                    while (off < resp_len && hart_resp[off] == 0xFF) off++;
                    size_t hart_data_len = resp_len - off;
                    if (hart_data_len > 0)
                    {
                        send_hart_response(client, hart_resp + off, hart_data_len, frame[4], frame[5]);
                        Serial.print("[UART] << ");
                        printHex(hart_resp + off, hart_data_len);
                    }
                    else
                    {
                        Serial.println("[WARN] Resposta HART sem dados após preâmbulo!");
                    }
                }
                else
                {
                    Serial.println("[WARN] Nenhuma resposta do modem HART");
                }
            }
            else if (msg_type == 0x00 && msg_id == 0x01)
            {
                Serial.println("[INFO] Session Close recebido. Fechando sessão.");
                client.stop(); // se preferir.
                break;         //
            }
            else
            {
                Serial.printf("[ERRO] Comando inválido. type=0x%02X id=0x%02X\n", msg_type, msg_id);
            }

            // Limpa buffer para próximo frame
            frame_len = 0;
        }
        client.stop();
        Serial.println("=== CONEXÃO TCP ENCERRADA ===");
    }
    delay(10);
}