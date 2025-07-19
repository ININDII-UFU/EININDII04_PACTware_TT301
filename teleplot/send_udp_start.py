import socket
ESP32_IP = "192.168.68.102"
ESP32_PORT = 8080       # Porta de destino na ESP32
LOCAL_PORT = 47269      # Porta de origem no PC

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', LOCAL_PORT))  # '' = qualquer IP local

sock.sendto(b"hello", (ESP32_IP, ESP32_PORT))
sock.close()