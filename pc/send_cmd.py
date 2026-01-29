import socket, sys
ip = sys.argv[1]
port = int(sys.argv[2])
msg = sys.argv[3].encode()
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(msg, (ip, port))
sock.settimeout(2)
try:
    data,_ = sock.recvfrom(1024)
    print(data.decode(errors="ignore").strip())
except Exception as e:
    print("no reply", e)
