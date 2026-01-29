# PC — Applications et scripts

Ce dossier contient les applications exécutées **côté PC**.
Elles permettent de communiquer avec la carte Zynq via **UDP**.

## Applications disponibles

### send_cmd
Outil en ligne de commande permettant d’envoyer une commande UDP simple vers la carte.

Commandes typiques :
- LedOn
- LedOff

Utilisation :
```bash
./send_cmd <IP_carte> <port> <commande>
```
Exemple :
```bash
./send_cmd 192.168.1.50 50000 LedOn
```

---

### udp_img_receiver

Script Python de réception d’image via UDP.
Les paquets sont reçus, puis l’image est reconstruite à partir des chunks.

Utilisation :
```bash
python udp_img_receiver.py <port> <image_sortie>
```

Exemple :
```bash
python udp_img_receiver.py 50001 image_recue.jpg
```

---

### IHM_microzed

Interface Homme-Machine côté PC.
Permet de piloter la carte Zynq de manière graphique via UDP.

Fonctionnalités :
- Envoi de commandes UDP
- Contrôle des fonctions de la carte
- Déclenchement de l’envoi d’image

Lancement :
```bash
python IHM_microzed.py
```

