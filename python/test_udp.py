import socket
import time

# Altere para o IP e porta da sua ESP32
ESP32_IP = "192.168.68.102"
ESP32_PORT = 47269

# Cria o socket UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Define tempo de espera (timeout)
sock.settimeout(3)

try:
    # Envia um pacote "hello" para a ESP32 (igual ao Teleplot)
    mensagem = b"hello"
    sock.sendto(mensagem, (ESP32_IP, ESP32_PORT))
    print(f"Enviado 'hello' para {ESP32_IP}:{ESP32_PORT}")

    # Aguarda resposta da ESP32
    while True:
        try:
            data, addr = sock.recvfrom(1024) # 1024 bytes de buffer
            print(f"Recebido de {addr}: {data.decode().strip()}")
            # Se quiser sair após o primeiro pacote, descomente:
            # break
        except socket.timeout:
            print("Nenhuma resposta recebida da ESP32 (timeout).")
            break

except Exception as e:
    print(f"Erro: {e}")

finally:
    sock.close()
