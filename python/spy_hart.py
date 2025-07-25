import socket
import time

ESP32_IP = "200.19.148.112"
ESP32_PORT = 5000
FRAME_TIMEOUT = 0.20  # 20 ms entre bytes encerra o frame

def hex_dump(data):
    return " ".join(f"{b:02X}" for b in data)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((ESP32_IP, ESP32_PORT))
    print(f"Conectado ao log da ESP32 ({ESP32_IP}:{ESP32_PORT})")
    buffer = b""
    last_prefix = None
    frame = b""
    last_time = time.time()

    while True:
        data = s.recv(1024)
        if not data:
            break
        buffer += data
        while b"\n" in buffer:
            line, buffer = buffer.split(b"\n", 1)
            now = time.time()
            try:
                if line.startswith(b"[TX] ") or line.startswith(b"[RX] "):
                    prefix = line[:5].decode()
                    payload = line[5:]
                    # Se mudou prefixo ou deu timeout, printa frame anterior
                    if (prefix != last_prefix or now - last_time > FRAME_TIMEOUT) and frame:
                        print(f"{last_prefix} {hex_dump(frame)}")
                        frame = b""
                    frame += payload
                    last_prefix = prefix
                    last_time = now
                else:
                    print(line.decode(errors='replace'))
            except Exception as e:
                print("[ERRO]", e, line)
        # Após sair do while interno, se ficou tempo sem novos bytes, fecha o frame
        if frame and (time.time() - last_time > FRAME_TIMEOUT):
            print(f"{last_prefix} {hex_dump(frame)}")
            frame = b""