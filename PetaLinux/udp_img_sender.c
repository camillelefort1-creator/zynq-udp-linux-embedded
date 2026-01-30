#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAGIC      0xCAFE
#define VERSION    1
#define CHUNK_SIZE 1400

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint8_t  version;
    uint8_t  flags;
    uint32_t frame_id;
    uint16_t chunk_id;
    uint16_t total_chunks;
    uint16_t payload_len;
} UdpImgHdr;
#pragma pack(pop)

int main(int argc, char **argv)
{
	
	//lecture des arguments
    if (argc < 4) {
        printf("Usage: %s <dest_ip> <dest_port> <file>\n", argv[0]); // IP de destination, port UDP, chemin du fichier à envoyer
        return 1;
    }

    const char *ip   = argv[1];
    int port         = atoi(argv[2]);
    const char *file = argv[3];

    // Ouvrir le fichier
    FILE *f = fopen(file, "rb");
    if (!f) { perror("fopen"); return 1; }

    // Taille du fichier 
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int total_chunks = (int)((size + CHUNK_SIZE - 1) / CHUNK_SIZE); // connaitre le nb de chuncks en fonction de la taille du fichier. dernier chunk peut être plus petit
	//Je viens d'apprendre qu'en C la division etait arrondie vers le bas. exemple => 2500 / 1400 = 1. Alors qu'il faut 2 chuncks. 
	//On ajoute CHUNK_SIZE - 1 pour forcer l’arrondi vers le haut quand il reste des octets.


    // Socket UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    inet_pton(AF_INET, ip, &dst.sin_addr);

    uint8_t payload[CHUNK_SIZE]; //buffer d'envoi. Playload : données lues depuis le fihciers
    uint8_t packet[sizeof(UdpImgHdr) + CHUNK_SIZE]; // packet => header + donnees

    uint32_t frame_id = 1; // simple : on met toujours 1

    for (int chunk_id = 0; chunk_id < total_chunks; chunk_id++) { //envoie du fichier morceau par morceau

        // Lire un morceau
        int r = fread(payload, 1, CHUNK_SIZE, f); // dernier chunk => r < CHUNK_SIZE. r est le nb d'octets lus

        // Header Conversion en network byte order pour compatibilité réseau.
        UdpImgHdr hdr;
        hdr.magic        = htons(MAGIC);
        hdr.version      = VERSION;
        hdr.flags        = 0;
        hdr.frame_id     = htonl(frame_id);
        hdr.chunk_id     = htons((uint16_t)chunk_id););
        hdr.total_chunks = htons((uint16_t)total_chunks);
        hdr.payload_len  = htons((uint16_t)r);

        // Construire packet = header + payload
        memcpy(packet, &hdr, sizeof(hdr));
        memcpy(packet + sizeof(hdr), payload, r);

        // Envoyer paquet au récepteur.
        sendto(sock, packet, sizeof(hdr) + r, 0,
               (struct sockaddr *)&dst, sizeof(dst));

        usleep(1000); // 1 ms
    }

    printf("File sent: %s (%ld bytes) in %d chunks\n", file, size, total_chunks);

    close(sock);
    fclose(f);
    return 0;
}

