import serial
import time
import re

PACTWARE_COM = 'COM9'     # Porta virtual onde o PACTware se conecta
ESP32_COM = 'COM8'        # Porta onde a ESP32 está conectada
BAUD = 115200
LOG_FILE = "bridge_ascii_to_esp32_binary_response_log.txt"

def log(msg):
    timestamp = time.strftime("[%H:%M:%S] ")
    print(timestamp + msg)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(timestamp + msg + "\n")

def byte_to_ascii_hex(byte_data):
    return ' '.join(f"{b:02X}" for b in byte_data) + '\n'

def parse_hex_line_to_bytes(hex_list):
    return bytes(int(h, 16) for h in hex_list)

def main():
    try:
        # Pactware COM normal com RTS/CTS ativo
        pact = serial.Serial(PACTWARE_COM, BAUD, timeout=0.1, rtscts=True)

        # ESP32 COM configurada com RTS/DTR desligados para evitar reset
        esp = serial.Serial()
        esp.port = ESP32_COM
        esp.baudrate = BAUD
        esp.timeout = 0.1
        esp.rtscts = False
        esp.dtr = False  # ← EVITA RESET
        esp.rts = False  # ← EVITA RESET
        esp.open()

        log("Ponte Pactware → ESP32 (ASCII) e ESP32 → Pactware (binário) iniciada...\n")

        buffer_esp = ""

        while True:
            # Ler comando binário do Pactware e enviar para ESP32 como texto
            if pact.in_waiting:
                data = pact.read(pact.in_waiting)
                ascii_cmd = byte_to_ascii_hex(data)
                esp.write(ascii_cmd.encode('ascii'))
                log(f"PACTWARE → ESP32: {ascii_cmd.strip()}")

            # Ler resposta ASCII da ESP32
            if esp.in_waiting:
                chunk = esp.read(esp.in_waiting).decode(errors='ignore')
                buffer_esp += chunk

                if '\n' in buffer_esp or '\r' in buffer_esp:
                    linhas = re.split('[\r\n]+', buffer_esp)
                    for linha in linhas[:-1]:
                        linha = linha.strip()

                        # Remove qualquer parte após "Enviando:"
                        if "Enviando:" in linha:
                            linha = linha.split("Enviando:")[0].strip()

                        # Extrair apenas hexadecimais válidos
                        hex_matches = re.findall(r'\b[0-9A-Fa-f]{2}\b', linha)

                        if len(hex_matches) >= 5:
                            raw = parse_hex_line_to_bytes(hex_matches)
                            pact.write(raw)
                            log(f"ESP32 → PACTWARE (hex): {linha} → {raw.hex(' ').upper()}")
                        elif linha:
                            log(f"ESP32 (ignorado): {linha}")

                    buffer_esp = linhas[-1]

            time.sleep(0.01)

    except Exception as e:
        log(f"[ERRO] {e}")

if __name__ == "__main__":
    main()