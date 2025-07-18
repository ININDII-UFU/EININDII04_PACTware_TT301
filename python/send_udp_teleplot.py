import socket
ESP32_IP = "192.168.68.102"
ESP32_PORT = 47269
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"hello", (ESP32_IP, ESP32_PORT))
sock.close()