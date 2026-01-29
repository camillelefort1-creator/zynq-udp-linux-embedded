import socket      
import struct      
import sys         
import time        

#  correspond au code C
MAGIC = 0xCAFE
VERSION = 1

# Format du header
# H  : uint16  -> magic
# B  : uint8   -> version
# B  : uint8   -> flags
# I  : uint32  -> frame_id
# H  : uint16  -> chunk_id
# H  : uint16  -> total_chunks
# H  : uint16  -> payload_len
# H  : uint16  -> reserved
HDR_FMT = "!HBBIHHHH"

# Taille totale du header octets
HDR_SIZE = struct.calcsize(HDR_FMT)

def main():
    # Vérification des arg
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <listen_port> <output_file>")
        print(f"Example: {sys.argv[0]} 50001 reconstructed.jpg")
        sys.exit(1)

    listen_port = int(sys.argv[1])
    out_path = sys.argv[2]

    # Création socket UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Écoute sur toutes les interfaces réseau
    sock.bind(("0.0.0.0", listen_port))
    sock.settimeout(2.0)  # Timeout pour éviter un blocage infini

    print(f"Listening UDP on port {listen_port} ...")

    # Variables de gestion de la frame 
    current_frame = None      # frame_id en cours
    chunks = {}               # dictionnaire {chunk_id: payload}
    total_chunks = None       # nombre total de chunks attendus
    t0 = None                 # temps de début de réception

    
    while True: # réception
        try:
            # Réception paquet UDP
            data, addr = sock.recvfrom(2048)  # suffisant pour header + payload
        except socket.timeout:
            
            continue # En cas de timeout on continue

        # Vérification taille min
        if len(data) < HDR_SIZE:
            continue

        # Décodage du header
        magic, ver, flags, frame_id, chunk_id, tot, payload_len, reserved = struct.unpack(
            HDR_FMT, data[:HDR_SIZE]
        )

        # Vérification du protocole
        if magic != MAGIC or ver != VERSION:
            continue

        # Extraction du payload
        payload = data[HDR_SIZE:HDR_SIZE + payload_len]
        if len(payload) != payload_len:
            continue

        # Détection une nouvelle frame
        if current_frame is None or frame_id != current_frame:
            current_frame = frame_id
            chunks = {}
            total_chunks = tot
            t0 = time.time()
            print(f"Receiving frame_id={frame_id} from {addr}, total_chunks={tot}")

        # Stockage du chunk (si pas déjà reçu)
        if chunk_id not in chunks:
            chunks[chunk_id] = payload

        # Affichage progression
        if total_chunks:
            got = len(chunks)
            if got % 20 == 0 or got == total_chunks:
                print(f"  got {got}/{total_chunks}")

        # Si tous les chunks sont reçus
        if total_chunks and len(chunks) == total_chunks:
            # Reconstruction du fichier dans l'ordre
            ordered = b"".join(chunks[i] for i in range(total_chunks))
            with open(out_path, "wb") as f:
                f.write(ordered)

            dt = time.time() - t0 if t0 else 0
            print(f"Frame complete: wrote {len(ordered)} bytes to {out_path} in {dt:.3f}s")

            # Réinitialisation pour la frame suivante
            current_frame = None
            chunks = {}
            total_chunks = None
            t0 = None

# Point d'entrée du programme
if __name__ == "__main__":
    main()

