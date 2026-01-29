#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CTRL_PORT 50000
#define BUF_SIZE 128

// LED D3 connectée au MIO 47
#define LED_GPIO 185

// Image à envoyer (simulation)
#define IMAGE_PATH "/usr/bin/image.jpg"   // adapte si besoin (ex: /mnt/sd/image.jpg)

// Protocole image
#define MAGIC 0xCAFE
#define VERSION 1
#define CHUNK_SIZE 1400
#define SEND_DELAY_US 1000  // 1 ms

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

// ---- SYSFS GPIO helpers ----
static int write_sysfs(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", value);
    fclose(f);
    return 0;
}

static void gpio_init(void) {
    char path[128];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", LED_GPIO);
    if (access(path, F_OK) != 0) {
        char num[16];
        snprintf(num, sizeof(num), "%d", LED_GPIO);
        write_sysfs("/sys/class/gpio/export", num);
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", LED_GPIO);
    write_sysfs(path, "out");

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", LED_GPIO);
    write_sysfs(path, "0");
}

static void led_set(int on) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", LED_GPIO);
    write_sysfs(path, on ? "1" : "0");
}

// ---- File helper ----
static long file_size(FILE *f) {
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long sz = ftell(f);
    if (sz < 0) return -1;
    if (fseek(f, 0, SEEK_SET) != 0) return -1;
    return sz;
}

// ---- Send image in chunks ----
static int send_image_chunks(int sock,
                             const struct sockaddr_in *dst,
                             socklen_t dst_len,
                             const char *path,
                             uint32_t frame_id)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen(image)");
        return -1;
    }

    long sz = file_size(f);
    if (sz <= 0) {
        fprintf(stderr, "Invalid image size: %ld\n", sz);
        fclose(f);
        return -1;
    }

    uint32_t total_chunks = (uint32_t)((sz + CHUNK_SIZE - 1) / CHUNK_SIZE);

    uint8_t payload[CHUNK_SIZE];
    uint8_t packet[sizeof(UdpImgHdr) + CHUNK_SIZE];

    for (uint32_t chunk_id = 0; chunk_id < total_chunks; chunk_id++) {
        size_t to_read = CHUNK_SIZE;
        long remaining = sz - (long)(chunk_id * CHUNK_SIZE);
        if (remaining < (long)CHUNK_SIZE) to_read = (size_t)remaining;

        size_t r = fread(payload, 1, to_read, f);
        if (r != to_read) {
            fprintf(stderr, "fread error chunk %u (got %zu exp %zu)\n",
                    chunk_id, r, to_read);
            fclose(f);
            return -1;
        }

        UdpImgHdr hdr;
        hdr.magic       = htons(MAGIC);
        hdr.version     = VERSION;
        hdr.flags       = 0;
        hdr.frame_id    = htonl(frame_id);
        hdr.chunk_id    = htons((uint16_t)chunk_id);
        hdr.total_chunks= htons((uint16_t)total_chunks);
        hdr.payload_len = htons((uint16_t)r);
        hdr.reserved    = 0;

        memcpy(packet, &hdr, sizeof(hdr));
        memcpy(packet + sizeof(hdr), payload, r);

        ssize_t sent = sendto(sock,
                              packet,
                              (ssize_t)(sizeof(hdr) + r),
                              0,
                              (const struct sockaddr *)dst,
                              dst_len);
        if (sent < 0) {
            perror("sendto(image chunk)");
            fclose(f);
            return -1;
        }

        usleep(SEND_DELAY_US);
    }

    printf("Sent image '%s' (%ld bytes) frame_id=%u chunks=%u to %s:%u\n",
           path, sz, frame_id, total_chunks,
           inet_ntoa(dst->sin_addr), ntohs(dst->sin_port));

    fclose(f);
    return 0;
}

// Parse "Request_img <port>"
static int parse_request_img(const char *buf, int *out_port) {
    const char *prefix = "Request_img";
    size_t n = strlen(prefix);
    if (strncmp(buf, prefix, n) != 0) return 0; // not this command

    // Accept both "Request_img" and "Request_img 50001"
    while (buf[n] == ' ') n++;
    if (buf[n] == '\0' || buf[n] == '\n' || buf[n] == '\r') {
        *out_port = 50001; // default if not provided
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
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CTRL_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("UDP control server listening on port %d\n", CTRL_PORT);
    printf("Commands: LedOn | LedOff | Request_img <port>\n");

    uint32_t frame_id = 1;

    while (1) {
        char buf[BUF_SIZE];
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        int n = recvfrom(sock, buf, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&client, &client_len);
        if (n <= 0) continue;
        buf[n] = '\0';

        // Clean CRLF
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) {
            buf[n-1] = '\0';
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
            int pr = parse_request_img(buf, &img_port);
            if (pr == 1) {
                // Envoi image vers client:img_port (et non le port source de la commande)
                struct sockaddr_in img_dst = client;
                img_dst.sin_port = htons((uint16_t)img_port);

                int rc = send_image_chunks(sock, &img_dst, sizeof(img_dst), IMAGE_PATH, frame_id++);
                if (rc == 0) {
                    sendto(sock, "OK\n", 3, 0, (struct sockaddr *)&client, client_len);
                } else {
                    sendto(sock, "ERR\n", 4, 0, (struct sockaddr *)&client, client_len);
                }
            } else if (pr == -1) {
                sendto(sock, "ERR\n", 4, 0, (struct sockaddr *)&client, client_len);
            } else {
                sendto(sock, "ERR\n", 4, 0, (struct sockaddr *)&client, client_len);
            }
        }
    }

    close(sock);
    return 0;
}
