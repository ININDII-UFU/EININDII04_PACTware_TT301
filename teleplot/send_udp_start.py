import os
import socket
import configparser

def get_local_ip():
    """Obtém o IP local principal (IPv4) da máquina."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Conecta a um IP público para forçar escolha da interface "principal"
        s.connect(('8.8.8.8', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

def read_kitid_from_ini(ini_path):
    """Lê o kitid do bloco [data] do ini"""
    config = configparser.ConfigParser()
    config.read(ini_path)
    try:
        kitid = config['data']['kitid']
        return kitid.strip()
    except Exception:
        return None

def main():
    print("=== Enviador de comando 'start' UDP para ESP32 ===")
    local_ip = get_local_ip()
    print(f"[INFO] IP local detectado: {local_ip}")

    # Caminho do ini: um diretório acima do script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    ini_path = os.path.abspath(os.path.join(script_dir, '..', 'platformio.ini'))
    print(f"[INFO] Procurando arquivo: {ini_path}")

    kitid = read_kitid_from_ini(ini_path)
    if kitid is None:
        print("[ERRO] Não foi possível ler 'kitid' do [data] no platformio.ini.")
        return

    hostname = f"inindkit{kitid}.local"
    port = 47269
    print(f"[INFO] Destino: {hostname}:{port}")

    # Resolve hostname para IP (usando mDNS, requer zeroconf/Bonjour ativo na rede)
    try:
        dest_ip = socket.gethostbyname(hostname)
        print(f"[INFO] Hostname resolvido para IP: {dest_ip}")
    except Exception as e:
        print(f"[ERRO] Não foi possível resolver o host '{hostname}': {e}")
        return

    # Envia o pacote UDP
    msg = b"start"
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(msg, (dest_ip, port))
        print(f"[OK] Comando 'start' enviado para {hostname} ({dest_ip}):{port}")
    except Exception as e:
        print(f"[ERRO] Falha ao enviar pacote UDP: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    main()