# Image PetaLinux — Carte SD

Ce dossier contient les fichiers à copier sur la **carte SD** utilisée pour démarrer la carte **Zynq / MicroZed** sous **PetaLinux 2014**.

## Contenu de la carte SD

Les fichiers suivants doivent être présents à la racine de la carte SD :

- `BOOT.bin`  
- `devicetree.dtb`  
- `uramdisk.image`  
- `image.ub` (si utilisé)

Ces fichiers sont générés lors de la construction de l’image PetaLinux.

---

## Fichiers applicatifs

La carte SD contient également :
- Les **binaires des applications Linux embarqué** (serveur UDP, envoi d’image, etc.)
- Les **images à transmettre vers le PC** via UDP

Ces fichiers sont copiés sur la carte afin d’être accessibles depuis Linux
(ex : montage de la SD dans `/mnt/sd`).

---

## Utilisation côté carte

Exemple de montage de la carte SD :
```bash
mkdir -p /mnt/sd
mount -t vfat /dev/mmcblk0p1 /mnt/sd
```
Exemple de lancement d’une application depuis la SD :
```bash
cd /mnt/sd
chmod +x udp_server
./udp_server
```

Notes
La compilation des binaires est réalisée sur PC (cross-compilation).
Aucun outil de compilation n’est utilisé directement sur la carte.
Le contenu de ce dossier correspond uniquement à la partie Linux embarqué.


---
