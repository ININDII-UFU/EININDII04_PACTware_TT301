import socket
ESP32_IP = "192.168.68.106"
ESP32_PORT = 47269
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"start", (ESP32_IP, ESP32_PORT))
sock.close()