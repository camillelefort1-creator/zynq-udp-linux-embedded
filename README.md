# Zynq UDP Linux Embedded Project

Projet de communication réseau UDP sur carte **Zynq / MicroZed** sous **PetaLinux 2014**.

Ce projet met en œuvre plusieurs applications Linux embarqué pour piloter la carte et échanger des données PC via Ethernet.

---

## Objectifs
- Mettre en place une communication UDP entre un PC et carte Zynq
- Transmettre une image via UDP depuis la carte vers le PC
- Développer une IHM PC pour piloter carte

> La partie FPGA / Vivado n’est pas détaillée dans ce dépôt.

---

## Architecture générale
- Carte : Zynq / MicroZed
- OS embarqué : PetaLinux 2014
- Réseau : Ethernet (IP statiques)
- Protocole : UDP
- Langages : C (embarqué), Python (PC)

---

## Structure du dépôt

```
zynq-udp-linux-embedded/
├── README.md
│
├── docs/
│   ├── Rapport_LEFORT_Projet_integrateur.pdf
│   └── Protocole_PetaLinux_2014.pdf
│
├── petalinux/
│   ├── blinq_led/
│   ├── udp_server/
│   └── udp_sender_image/
│
├── pc/
│   ├── send_cmd
│   ├── udp_img_receiver.py
│   └── IHM_microzed/
│
└── Image_PetaLinuxSD/
```


---

## Applications

### Côté carte (PetaLinux)
- **Blinq LED** : test GPIO et clignotement de LED
- **UDP server** : serveur UDP de commande (LedOn / LedOff)
- **UDP sender image** : envoi d’image via UDP vers le PC

📁 Voir `petalinux/`

---

### Côté PC
- **send_cmd** : envoi de commandes UDP en ligne de commande
- **udp_img_receiver** : réception et reconstruction d’image
- **IHM_microzed** : interface graphique de contrôle de la carte

📁 Voir `pc/`

---

## Documentation
- 📄 [Rapport du projet](docs/Rapport_LEFORT_Projet_integrateur.pdf)
- 📄 [Protocole d'utilisation PetaLinux 2014](docs/Protocole_PetaLinux_2014.pdf)

---

## Lancement rapide

### Configuration réseau (exemple carte)
```bash
ifconfig eth0 192.168.1.50 netmask 255.255.255.0 up
```
Exemple : allumer une LED depuis le PC
```bash
./send_cmd 192.168.1.50 50000 LedOn
```
Outils utilisés :
- PetaLinux 2014
- GCC (cross-compilation)
- Python 3
- Wireshark

Auteur
Projet réalisé par camille.lefort@etu.univ-grenoble-alpes.fr
Projet intégrateur — Linux embarqué sur Zynq
