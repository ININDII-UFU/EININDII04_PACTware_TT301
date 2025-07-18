import socket

ESP32_IP = "192.168.68.102"   # Coloque o IP da ESP32 que aparece no Teleplot
ESP32_PORT = 47269            # Porta do Teleplot/ESP32

mensagem = "dado1:25.0|"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(mensagem.encode(), (ESP32_IP, ESP32_PORT))
print(f"Enviado para {ESP32_IP}:{ESP32_PORT} --> {mensagem}")
sock.close()