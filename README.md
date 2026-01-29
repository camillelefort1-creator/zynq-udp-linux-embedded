# Zynq UDP Linux Embedded Project

Projet de communication réseau UDP sur carte **Zynq / MicroZed** sous **PetaLinux 2014**.

Ce projet va mettre en oeuvre plusieurs applications Linux pour piloter la carte et échanger des données PC via Ethernet.

---

## Objectifs
- Mettre en place une communication UDP entre un PC et carte Zynq
- Transmettre une image via UDP depuis la carte vers le PC

> La partie FPGA / Vivado n’est pas détaillée dans ce dépôt.

---

## Architecture
- Carte : Zynq / MicroZed
- OS embarqué : PetaLinux 2014
- Réseau : Ethernet (IP statiques)
- Protocole : UDP
- Langages : C (embarqué), Python (PC)

---

## Navigation du projet

- [Documentation (rapport & protocole)](docs/)
- [Applications Linux embarqué (PetaLinux)](PetaLinux/)
- [Applications PC](pc/)
- [Image PetaLinux / Carte SD](Image_PetaLinux_SD/)

---

## Applications

### Côté carte (PetaLinux)
- **Blinq LED** : test GPIO et clignotement de LED
- **UDP server** : serveur UDP de commande (LedOn / LedOff)
- **UDP sender image** : envoi d’image via UDP vers le PC

Voir `petalinux/`

---

### Côté PC
- **send_cmd** : envoi de commandes UDP en ligne de commande
- **udp_img_receiver** : réception et reconstruction d’image
- **IHM_microzed** : interface graphique de contrôle de la carte

Voir `pc/`

---

## Documentation
- 📄 [Rapport du projet](docs/Rapport_LEFORT_Projet_integrateur.pdf)
- 📄 [Présentation](docs/Projet_présentation.pdf)
- 📄 [Protocole d'utilisation PetaLinux 2014](docs/Protocole_PetaLinux_2014.pdf)

---
Outils utilisés :
- PetaLinux 2014
- GCC (cross-compilation)
- Python 3
- Wireshark


- Projet réalisé par camille.lefort@etu.univ-grenoble-alpes.fr
- Projet intégrateur — Linux embarqué sur Zynq
