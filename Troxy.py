import socket
import threading
import time
import json
import struct

# Configuration
CONFIG = {
    'server_ip': '',              # Set via input or hardcode (e.g., 't.dark-gaming.com')
    'server_port': 7777,          # Set via input or hardcode
    'server_name': 'TerrariaProxy',  # Name displayed on Xbox (max 16 chars recommended)
    'local_broadcast_ip': '',     # Determined dynamically
    'port_8888': 8888,            # UDP port for discovery
    'port_7777': 7777,            # TCP and UDP port for gameplay
    'broadcast_interval': 5,       # Seconds between broadcasts
    'tcp_timeout': 30,            # Seconds for TCP connection timeout
    'server_check_interval': 10   # Seconds between server checks
}

# State
STATE = {
    'server_reachable': False,
    'server_status': 'Unknown',
    'server_name': 'Unknown',     # From server status response
    'player_count': 0,
    'max_players': 0
}

# Terraria discovery packet (corrected spelling)
# Format: [0xf2, 0x03, 0x00, 0x00] (header) + [0x61, 0x1e, 0x00, 0x00] (unknown) +
# [0x08] (short name length) + short name (e.g., "Terraria") +
# [0x0d] (full name length) + full name (e.g., "TerrariaProxy") +
# [0x68, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00] (footer)
def build_response_packet(server_name):
    """Build discovery packet with custom server name."""
    short_name = server_name[:8]  # First 8 chars for short name
    full_name = server_name[:16]  # Max 16 chars for full name
    packet = [
        0xf2, 0x03, 0x00, 0x00,
        0x61, 0x1e, 0x00, 0x00,
        len(short_name)
    ] + [ord(c) for c in short_name] + [
        len(full_name)
    ] + [ord(c) for c in full_name] + [
        0x68,
        0x10, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x10, 0x00, 0x00
    ]
    return bytes(packet)

RESPONSE_PACKET = build_response_packet(CONFIG['server_name'])

# Terraria status request packet
STATUS_REQUEST_PACKET = bytes([0x03, 0x00, 0x01])

def is_discovery_packet(buf):
    """Check if a packet is a Terraria discovery packet."""
    return len(buf) >= 4 and buf[:4] == bytes([0xf2, 0x03, 0x00, 0x00])

def get_local_ip():
    """Get the local IP address."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('8.8.8.8', 80))
        local_ip = s.getsockname()[0]
    except Exception:
        local_ip = '127.0.0.1'
    finally:
        s.close()
    return local_ip

def get_broadcast_ip(local_ip):
    """Calculate the broadcast IP from the local IP (assuming /24 subnet)."""
    parts = local_ip.split('.')
    if len(parts) == 4:
        return f"{parts[0]}.{parts[1]}.{parts[2]}.255"
    return '255.255.255.255'

def check_server_connection():
    """Check if the server is reachable and fetch name/player count."""
    # First, check TCP connectivity
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2)
    try:
        s.connect((CONFIG['server_ip'], CONFIG['server_port']))
        STATE['server_reachable'] = True
        STATE['server_status'] = '✓ Server Online'
    except Exception:
        STATE['server_reachable'] = False
        STATE['server_status'] = '✗ Server Offline'
        STATE['server_name'] = 'Unknown'
        STATE['player_count'] = 0
        STATE['max_players'] = 0
        print(f"Server status: {STATE['server_status']}")
        s.close()
        return
    finally:
        s.close()
    
    # Send status request via UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2)
    try:
        sock.sendto(STATUS_REQUEST_PACKET, (CONFIG['server_ip'], CONFIG['server_port']))
        data, _ = sock.recvfrom(1024)
        if len(data) > 3 and data[2] == 0x03:  # Status response
            status_str = data[3:].decode('utf-8', errors='ignore')
            try:
                status_json = json.loads(status_str)
                STATE['server_name'] = status_json.get('name', 'Unknown')
                STATE['player_count'] = status_json.get('playercount', 0)
                STATE['max_players'] = status_json.get('maxplayers', 0)
            except json.JSONDecodeError:
                STATE['server_name'] = 'Unknown'
                STATE['player_count'] = 0
                STATE['max_players'] = 0
        else:
            STATE['server_name'] = 'Unknown'
            STATE['player_count'] = 0
            STATE['max_players'] = 0
    except Exception as e:
        print(f"Status request failed: {e}")
        STATE['server_name'] = 'Unknown'
        STATE['player_count'] = 0
        STATE['max_players'] = 0
    finally:
        sock.close()
    
    print(f"Server status: {STATE['server_status']} | Name: {STATE['server_name']} | Players: {STATE['player_count']}/{STATE['max_players']}")

def send_broadcast():
    """Broadcast server presence to local network."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(('', 0))
    while True:
        try:
            sock.sendto(RESPONSE_PACKET, (CONFIG['local_broadcast_ip'], CONFIG['port_8888']))
            print(f"Broadcasted to {CONFIG['local_broadcast_ip']}:{CONFIG['port_8888']}")
            time.sleep(CONFIG['broadcast_interval'])
        except Exception as e:
            print(f"Broadcast error: {e}")
            break
    sock.close()

def forward_udp(port, label):
    """Forward UDP packets on specified port."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(('', port))
    except OSError as e:
        print(f"Cannot bind UDP port {port} ({label}): {e}")
        print(f"{label} forwarding disabled. Run proxy on a different PC/iPhone if port is in use.")
        return
    sock.settimeout(1)
    out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"Listening for UDP on port {port} ({label})")
    while True:
        try:
            data, addr = sock.recvfrom(512)
            print(f"{label} received from {addr}: {len(data)} bytes")
            
            if is_discovery_packet(data):
                sock.sendto(RESPONSE_PACKET, addr)
                print(f"Sent discovery response to {addr}")
                continue
                
            if not STATE['server_reachable']:
                print(f"{label}: Server unreachable - dropping packet")
                continue
                
            out_sock.sendto(data, (CONFIG['server_ip'], CONFIG['server_port']))
            try:
                out_sock.settimeout(1)
                response, _ = out_sock.recvfrom(512)
                sock.sendto(response, addr)
            except socket.timeout:
                pass
        except socket.timeout:
            continue
        except Exception as e:
            print(f"{label} error: {e}")
            break
    sock.close()
    out_sock.close()

def forward_tcp():
    """Forward TCP connections to the server."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(('', CONFIG['port_7777']))
    except OSError as e:
        print(f"Cannot bind TCP port {CONFIG['port_7777']}: {e}")
        print("TCP forwarding disabled. Run proxy on a different PC/iPhone to use TCP 7777.")
        return
    sock.listen(5)
    sock.settimeout(1)
    
    print(f"TCP listening on port {CONFIG['port_7777']}")
    while True:
        try:
            client, addr = sock.accept()
            print(f"TCP connection from {addr}")
            
            if not STATE['server_reachable']:
                client.close()
                continue
                
            server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                server.connect((CONFIG['server_ip'], CONFIG['server_port']))
            except Exception:
                print(f"Failed to connect to server {CONFIG['server_ip']}:{CONFIG['server_port']}")
                client.close()
                server.close()
                continue
                
            client.settimeout(0.1)
            server.settimeout(0.1)
            last_active = time.time()
            
            while True:
                try:
                    data = client.recv(1024)
                    if not data:  # Client disconnected
                        break
                    server.sendall(data)
                    last_active = time.time()
                except socket.timeout:
                    pass
                except Exception:
                    break
                
                try:
                    data = server.recv(1024)
                    if not data:  # Server disconnected
                        break
                    client.sendall(data)
                    last_active = time.time()
                except socket.timeout:
                    pass
                except Exception:
                    break
                
                if time.time() - last_active > CONFIG['tcp_timeout']:
                    print(f"TCP connection timed out for {addr}")
                    break
                    
            client.close()
            server.close()
            print(f"TCP connection closed for {addr}")
        except socket.timeout:
            continue
        except Exception as e:
            print(f"TCP error: {e}")
            break
    sock.close()

def main():
    # Prompt for server IP, port, and name
    print("Enter the public Terraria server IP or hostname (e.g., t.dark-gaming.com):")
    CONFIG['server_ip'] = input().strip()
    print(f"Enter the server port (default {CONFIG['server_port']}):")
    port_input = input().strip()
    if port_input:
        try:
            CONFIG['server_port'] = int(port_input)
        except ValueError:
            print(f"Invalid port, using default {CONFIG['server_port']}")
    RESPONSE_PACKET = build_response_packet(CONFIG['server_name'])
    
    # Set local broadcast IP
    local_ip = get_local_ip()
    CONFIG['local_broadcast_ip'] = get_broadcast_ip(local_ip)
    print(f"Using local IP: {local_ip}, broadcast IP: {CONFIG['local_broadcast_ip']}")
    
    # Check server connection and status
    check_server_connection()
    
    # Start broadcast thread
    broadcast_thread = threading.Thread(target=send_broadcast, daemon=True)
    broadcast_thread.start()
    
    # Start UDP forwarding threads
    udp_8888_thread = threading.Thread(target=forward_udp, args=(CONFIG['port_8888'], 'UDP 8888'), daemon=True)
    udp_8888_thread.start()
    udp_7777_thread = threading.Thread(target=forward_udp, args=(CONFIG['port_7777'], 'UDP 7777'), daemon=True)
    udp_7777_thread.start()
    
    # Start TCP forwarding thread
    tcp_thread = threading.Thread(target=forward_tcp, daemon=True)
    tcp_thread.start()
    
    # Periodic server status check
    try:
        while True:
            check_server_connection()
            time.sleep(CONFIG['server_check_interval'])
    except KeyboardInterrupt:
        print("Shutting down proxy...")

if __name__ == "__main__":
    main()
