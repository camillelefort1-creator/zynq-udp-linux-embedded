/*
 *  serveur UDP  :
 * - écoute sur CTRL_PORT
 * - commandes: LedOn / LedOff / Request_img [port]
 * - si Request_img: envoie un fichier en chunks en UDP au port demandé
 *
 * Remarque: j'utilise sysfs GPIO parce que c'est plus simple à tester.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CTRL_PORT 50000
#define BUF_SIZE 128

// LED D3 connectée au MIO 47 -> GPIO 185 
#define LED_GPIO 185

// Fichier à envoyer
#define IMAGE_PATH "/usr/bin/image.jpg"  // à adapter

// protocole 
#define MAGIC 0xCAFE
#define VERSION 1
#define CHUNK_SIZE 1400
#define SEND_DELAY_US 1000 // 1ms (évite de spam le réseau)

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint8_t  version;
    uint8_t  flags;
    uint32_t frame_id;
    uint16_t chunk_id;
    uint16_t total_chunks;
    uint16_t payload_len;
    uint16_t reserved;
} UdpImgHdr;
#pragma pack(pop)

// SYSFS GPIO 
static int sysfs_write(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", value);
    fclose(f);
    return 0;
}

static void gpio_init(void) {
    char path[128];
    char num[16];

    // export si pas déjà fait
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", LED_GPIO);
    if (access(path, F_OK) != 0) {
        snprintf(num, sizeof(num), "%d", LED_GPIO);
        sysfs_write("/sys/class/gpio/export", num);
        // petit délai au cas où le kernel crée le dossier après
        usleep(10000);
    }

    // direction = out
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", LED_GPIO);
    sysfs_write(path, "out");

    // LED éteinte au début
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", LED_GPIO);
    sysfs_write(path, "0");
}

static void led_set(int on) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", LED_GPIO);
    sysfs_write(path, on ? "1" : "0");
}

// Fichier
static long get_file_size(FILE *f) {
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long sz = ftell(f);
    if (sz < 0) return -1;
    if (fseek(f, 0, SEEK_SET) != 0) return -1;
    return sz;
}

//Envoi image en chunks
static int send_image(int sock,
                      const struct sockaddr_in *dst,
                      socklen_t dst_len,
                      const char *path,
                      uint32_t frame_id)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return -1;
    }

    long sz = get_file_size(f);
    if (sz <= 0) {
        fprintf(stderr, "Taille fichier bizarre: %ld\n", sz);
        fclose(f);
        return -1;
    }

    uint32_t total_chunks = (uint32_t)((sz + CHUNK_SIZE - 1) / CHUNK_SIZE);

    uint8_t payload[CHUNK_SIZE];
    uint8_t packet[sizeof(UdpImgHdr) + CHUNK_SIZE];

    for (uint32_t chunk_id = 0; chunk_id < total_chunks; chunk_id++) {

        long remaining = sz - (long)(chunk_id * CHUNK_SIZE);
        size_t to_read = (remaining < (long)CHUNK_SIZE) ? (size_t)remaining : (size_t)CHUNK_SIZE;

        size_t r = fread(payload, 1, to_read, f);
        if (r != to_read) {
            fprintf(stderr, "Erreur fread chunk %u (got %zu, exp %zu)\n",
                    chunk_id, r, to_read);
            fclose(f);
            return -1;
        }

        // header
        UdpImgHdr hdr;
        hdr.magic        = htons(MAGIC);
        hdr.version      = VERSION;
        hdr.flags        = 0;
        hdr.frame_id     = htonl(frame_id);
        hdr.chunk_id     = htons((uint16_t)chunk_id);
        hdr.total_chunks = htons((uint16_t)total_chunks);
        hdr.payload_len  = htons((uint16_t)r);
        hdr.reserved     = 0;

        memcpy(packet, &hdr, sizeof(hdr));
        memcpy(packet + sizeof(hdr), payload, r);

        ssize_t sent = sendto(sock,
                              packet,
                              (ssize_t)(sizeof(hdr) + r),
                              0,
                              (const struct sockaddr *)dst,
                              dst_len);
        if (sent < 0) {
            perror("sendto");
            fclose(f);
            return -1;
        }

        usleep(SEND_DELAY_US);
    }

    printf("Image envoyée: %s (%ld bytes) frame=%u chunks=%u -> %s:%u\n",
           path, sz, frame_id, total_chunks,
           inet_ntoa(dst->sin_addr), ntohs(dst->sin_port));

    fclose(f);
    return 0;
}

// "Request_img <port>" (port optionnel)
static int parse_request_img(const char *buf, int *out_port) {
    const char *prefix = "Request_img";
    size_t n = strlen(prefix);

    if (strncmp(buf, prefix, n) != 0) return 0; // pas la commande

    while (buf[n] == ' ') n++;

    // si pas de port -> par défaut
    if (buf[n] == '\0') {
        *out_port = 50001;
        return 1;
    }

    int port = atoi(buf + n);
    if (port <= 0 || port > 65535) return -1;
    *out_port = port;
    return 1;
}

int main(void) {
    gpio_init();

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(CTRL_PORT);
    srv.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("Serveur UDP sur port %d\n", CTRL_PORT);
    printf("Cmd: LedOn | LedOff | Request_img [port]\n");

    uint32_t frame_id = 1;

    while (1) {
        char buf[BUF_SIZE];
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        int n = recvfrom(sock, buf, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&client, &client_len);
        if (n <= 0) continue;

        buf[n] = '\0';

        // enlève \n \r à la fin
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
            buf[n - 1] = '\0';
            n--;
        }

        if (strcmp(buf, "LedOn") == 0) {
            led_set(1);
            sendto(sock, "OK\n", 3, 0, (struct sockaddr *)&client, client_len);
        }
        else if (strcmp(buf, "LedOff") == 0) {
            led_set(0);
            sendto(sock, "OK\n", 3, 0, (struct sockaddr *)&client, client_len);
        }
        else {
            int img_port = 0;
            int r = parse_request_img(buf, &img_port);

            if (r == 1) {
                // on renvoie l'image vers l'IP du client, mais sur le port demandé
                struct sockaddr_in dst = client;
                dst.sin_port = htons((uint16_t)img_port);

                int rc = send_image(sock, &dst, sizeof(dst), IMAGE_PATH, frame_id++);
                if (rc == 0) sendto(sock, "OK\n", 3, 0, (struct sockaddr *)&client, client_len);
                else         sendto(sock, "ERR\n", 4, 0, (struct sockaddr *)&client, client_len);
            } else {
                // commande inconnue ou port invalide
                sendto(sock, "ERR\n", 4, 0, (struct sockaddr *)&client, client_len);
            }
        }
    }

    close(sock);
    return 0;
}
