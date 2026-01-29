#!/usr/bin/env python3
#
#Petit script UDP très basique pour envoyer une commande
#à un serveur (genre TP réseau / embarqué).
#
#Usage:
#    python3 udp_client.py <ip> <port> <message>
#
#Ex:
#    python3 udp_client.py 192.168.1.10 50000 LedOn
#

import socket
import sys

# récup des arguments
ip = sys.argv[1]
port = int(sys.argv[2])
msg = sys.argv[3].encode()  # on envoie en bytes

# création socket UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# envoi du message
sock.sendto(msg, (ip, port))

# timeout pour éviter de bloquer si pas de réponse
sock.settimeout(2)

try:
    data, _ = sock.recvfrom(1024)
    print(data.decode(errors="ignore").strip())
except Exception as e:
    # si le serveur répond pas
    print("no reply", e)

sock.close()
