#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define UDP_PORT 50000
#define BUF_SIZE  64
#define LED_GPIO  185

// Écrit une chaîne dans un fichier sysfs
static void sysfs_write(const char *path, const char *value)
{
    FILE *f = fopen(path, "w");
    if (!f) return;          // on ignore les erreurs
    fputs(value, f);
    fclose(f);
}

// Prépare le GPIO (export + direction out + LED off)
static void gpio_init(void)
{
    char path[128]; // vraiment large en taille, un fichier depasse rarement 40 caracteres, soyons prudents (64 un peu fragile)
    char num[16]; // large aussi en taille pour num

    // export
    snprintf(num, sizeof(num), "%d", LED_GPIO); //snprintf écrit une chaîne de caractères formatée dans un buffer, sans dépasser sa taille.
    sysfs_write("/sys/class/gpio/export", num);

    // direction = out
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", LED_GPIO);
    sysfs_write(path, "out");

    // value = 0 (off)
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", LED_GPIO);
    sysfs_write(path, "0");
}

// Met la LED à 1 ou 0
static void led_set(int on)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", LED_GPIO);
    sysfs_write(path, on ? "1" : "0");
}

int main(void)
{
    int sock; //distributeur de socket udp (fichier reseau)
    struct sockaddr_in addr, client; // addr, adresse du serveur, client adresse du client qui envoie udp 
    socklen_t client_len = sizeof(client);//client_len taille de la structure client
    char buf[BUF_SIZE]; // buffer pour stocker le message recu, ledon ledoff

    gpio_init();

    // Socket UDP
    sock = socket(AF_INET, SOCK_DGRAM, 0); //cree un socjet, af_inet => ipv4 et sock_dgram => udp avec 0 protocole par defaut


	//config de l'adresse du serveur
    memset(&addr, 0, sizeof(addr)); // on commence par tout mettre a 0
    addr.sin_family = AF_INET; //IPv4
    addr.sin_port = htons(UDP_PORT); // sin_port port udp, convertir en format reseau
    addr.sin_addr.s_addr = htonl(INADDR_ANY);// INADDR_ANY accepte les paquets venant de toute interface reseau

    bind(sock, (struct sockaddr *)&addr, sizeof(addr)); // ecoute sur le port 5000

    printf("UDP server on port %d (LedOn / LedOff)\n", UDP_PORT);

    while (1) {
        int n = recvfrom(sock, buf, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&client, &client_len); // reception message udp, recoit dans buf, recupere l'adresse du client dans client, n = nombre octet
        if (n <= 0) continue; // si rien recu on attend autre message

        buf[n] = '\0'; //transforme le buffer en chaine C valide

        if (strcmp(buf, "LedOn") == 0) { //led on
            led_set(1);
            sendto(sock, "OK\n", 3, 0, (struct sockaddr *)&client, client_len);
        }
        else if (strcmp(buf, "LedOff") == 0) { //led of
            led_set(0);
            sendto(sock, "OK\n", 3, 0, (struct sockaddr *)&client, client_len);
        }
        else { //erreur
            sendto(sock, "ERR\n", 4, 0, (struct sockaddr *)&client, client_len);
        }
    }

    close(sock);//lebere le socjet (boucle wile 1 donc jamais atteint)
    return 0;
}
