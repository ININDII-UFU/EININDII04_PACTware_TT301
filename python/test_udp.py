import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 47269))
print("Escutando UDP 47269...")
while True:
    data, addr = sock.recvfrom(1024)
    print("Recebido:", data, "de", addr)
