import socket
import configparser
from pathlib import Path

# === CONFIGURAÇÃO INICIAL ===
CONFIG_FILENAME = 'wokwi.toml'
config_path = Path(__file__).resolve().parent / CONFIG_FILENAME

config = configparser.ConfigParser()
config.read(config_path)

kitid = config.getint('wokwi', 'telnetPort', fallback=4000) - 4000

TEP_PORT = 47269 if kitid == 0 else 47270 + kitid
TEP_HOST = '127.0.0.1'
TELNET_PORT = 4000 + kitid
TELNET_HOST = '192.168.68.112'

# === FIM DAS CONFIGURAÇÕES ===

print(f"Using config file: {config_path}")
print(f"kitid = {kitid}, forwarding TCP {TELNET_HOST}:{TELNET_PORT} → UDP {TEP_HOST}:{TEP_PORT}")

# Abre conexão TCP (simples, substitui telnetlib)
tcp_sock = socket.create_connection((TELNET_HOST, TELNET_PORT))
tcp_file = tcp_sock.makefile('rb')  # leitura binária, linha a linha

# Prepara socket UDP
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    while True:
        line = tcp_file.readline()
        if not line:
            continue
        if not line.startswith(b'>'):
            continue
        payload = line.lstrip(b'>')
        udp_sock.sendto(payload, (TEP_HOST, TEP_PORT))
except KeyboardInterrupt:
    print("\nStopping proxy.")
finally:
    tcp_file.close()
    tcp_sock.close()
    udp_sock.close()