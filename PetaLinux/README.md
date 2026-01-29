# PetaLinux (2014) — Applications C

Ce dossier contient les applications **C** exécutées sur la carte Zynq sous **PetaLinux 2014**.

## Applications disponibles


### 1) UDP server (commande LED)
- Serveur UDP côté carte.
- Attend des commandes ASCII :
  - `LedOn`
  - `LedOff`
- Retour attendu : `OK` (ou `ERR`)

 Fichier : `udp_led/`

---

### 2) UDP sender image
- Envoi d’une image depuis la carte vers le PC via UDP.
- Découpage en paquets pour éviter la fragmentation réseau.
- Utilisé avec le script Python de réception côté PC.

Fichier : `udp_img_server/`

---

### 2) UDP serveur controle
- Envoi d’une image depuis la carte vers le PC via UDP.
- Changement état led D3 en fonction de commande UDP.
- Utilisé avec le script Python IHM côté PC.

Fichier : `udp_ctrl_server/`

---

## ▶️ Exécution rappel

> Exemple : lancer une application depuis la SD

```bash
mount -t vfat /dev/mmcblk0p1 /mnt/sd
cd /mnt/sd
chmod +x <binaire>
./<binaire>
