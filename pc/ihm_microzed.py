import os
import socket
import struct
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk

# --- Réseau (adapte si besoin) ---
CARD_IP = "192.168.1.50"
CTRL_PORT = 50000

LISTEN_IP = "0.0.0.0"
IMG_PORT = 50001
OUT_IMAGE = "reconstructed.jpg"

# --- Protocole image ---
MAGIC = 0xCAFE
VERSION = 1
HDR_FMT = "!HBBIHHHH"   # 16 bytes
HDR_SIZE = struct.calcsize(HDR_FMT)


class MicroZedGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("MicroZed UDP Control (LED + Image)")

        self.photo = None  # ImageTk reference (avoid GC)
        self.status_var = tk.StringVar(value="Prêt.")
        self.path_var = tk.StringVar(value=os.path.abspath(OUT_IMAGE))

        # --- Layout ---
        top = tk.Frame(root)
        top.pack(padx=10, pady=10, fill="x")

        tk.Label(top, text="Image affichée:").grid(row=0, column=0, sticky="w")
        tk.Entry(top, textvariable=self.path_var, width=60).grid(row=0, column=1, padx=5, sticky="we")
        top.grid_columnconfigure(1, weight=1)

        btns = tk.Frame(root)
        btns.pack(padx=10, pady=5, fill="x")

        tk.Button(btns, text="Choisir une image…", command=self.choose_image).grid(row=0, column=0, padx=5, pady=5)
        tk.Button(btns, text="Afficher", command=self.refresh_image).grid(row=0, column=1, padx=5, pady=5)

        tk.Button(btns, text="LED ON", width=12, command=lambda: self.send_cmd("LedOn")).grid(row=0, column=2, padx=5, pady=5)
        tk.Button(btns, text="LED OFF", width=12, command=lambda: self.send_cmd("LedOff")).grid(row=0, column=3, padx=5, pady=5)

        tk.Button(btns, text="Request_img", width=14, command=self.request_image).grid(row=0, column=4, padx=5, pady=5)

        # Zone image
        self.img_label = tk.Label(root, text="Image manquante", width=80, height=25, bg="#222", fg="white")
        self.img_label.pack(padx=10, pady=10, fill="both", expand=True)

        # Status bar
        status = tk.Frame(root)
        status.pack(fill="x", padx=10, pady=(0, 10))
        tk.Label(status, textvariable=self.status_var, anchor="w").pack(fill="x")

        # Affichage initial
        self.refresh_image()

    # --- UI helpers ---
    def set_status(self, txt):
        self.status_var.set(txt)

    def choose_image(self):
        path = filedialog.askopenfilename(
            title="Choisir une image",
            filetypes=[("Images", "*.jpg *.jpeg *.png *.bmp"), ("Tous fichiers", "*.*")]
        )
        if path:
            self.path_var.set(path)
            self.refresh_image()

    def refresh_image(self):
        path = self.path_var.get().strip()
        if not path or not os.path.exists(path):
            self.photo = None
            self.img_label.configure(image="", text="Image manquante")
            self.set_status("Aucune image à afficher (fichier introuvable).")
            return

        try:
            img = Image.open(path)
            # redimensionne pour rentrer dans la fenêtre (conserve ratio)
            w = self.img_label.winfo_width()
            h = self.img_label.winfo_height()
            if w <= 1 or h <= 1:
                w, h = 900, 600
            img.thumbnail((w - 20, h - 20))

            self.photo = ImageTk.PhotoImage(img)
            self.img_label.configure(image=self.photo, text="")
            self.set_status(f"Image affichée: {path}")
        except Exception as e:
            self.photo = None
            self.img_label.configure(image="", text="Erreur affichage image")
            self.set_status(f"Erreur: {e}")

    # --- UDP control commands ---
    def send_cmd(self, cmd):
        def worker():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.settimeout(2.0)
                sock.sendto(cmd.encode(), (CARD_IP, CTRL_PORT))
                try:
                    data, _ = sock.recvfrom(1024)
                    rep = data.decode(errors="ignore").strip()
                except socket.timeout:
                    rep = "(pas de réponse)"
                finally:
                    sock.close()

                self.root.after(0, lambda: self.set_status(f"Commande '{cmd}' -> {rep}"))
            except Exception as e:
                self.root.after(0, lambda: self.set_status(f"Erreur commande '{cmd}': {e}"))

        threading.Thread(target=worker, daemon=True).start()

    # --- Request image (receive + display) ---
    def request_image(self):
        """
        Envoie "Request_img <IMG_PORT>" à la carte
        puis écoute sur IMG_PORT pour reconstruire OUT_IMAGE,
        puis affiche OUT_IMAGE automatiquement.
        """
        self.set_status("Request_img: démarrage réception…")

        def worker():
            # 1) préparer socket de réception image
            rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            rx.bind((LISTEN_IP, IMG_PORT))
            rx.settimeout(2.0)

            # 2) envoyer commande request_img
            try:
                tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                tx.settimeout(2.0)
                cmd = f"Request_img {IMG_PORT}"
                tx.sendto(cmd.encode(), (CARD_IP, CTRL_PORT))
                # on lit la réponse OK/ERR (pas obligatoire)
                try:
                    data, _ = tx.recvfrom(1024)
                    rep = data.decode(errors="ignore").strip()
                except socket.timeout:
                    rep = "(pas de réponse)"
                tx.close()
            except Exception as e:
                rx.close()
                self.root.after(0, lambda: self.set_status(f"Erreur envoi Request_img: {e}"))
                return

            self.root.after(0, lambda: self.set_status(f"Commande Request_img envoyée -> {rep}. Réception UDP…"))

            # 3) recevoir & reconstruire
            current_frame = None
            total_chunks = None
            chunks = {}
            start = time.time()

            # délai max pour une frame complète
            deadline = time.time() + 8.0

            try:
                while time.time() < deadline:
                    try:
                        data, addr = rx.recvfrom(2048)
                    except socket.timeout:
                        continue

                    if len(data) < HDR_SIZE:
                        continue

                    magic, ver, flags, frame_id, chunk_id, tot, payload_len, reserved = struct.unpack(
                        HDR_FMT, data[:HDR_SIZE]
                    )
                    if magic != MAGIC or ver != VERSION:
                        continue

                    payload = data[HDR_SIZE:HDR_SIZE + payload_len]
                    if len(payload) != payload_len:
                        continue

                    # nouvelle frame détectée
                    if current_frame is None or frame_id != current_frame:
                        current_frame = frame_id
                        total_chunks = tot
                        chunks = {}
                        start = time.time()
                        self.root.after(0, lambda: self.set_status(
                            f"Réception image frame_id={frame_id} depuis {addr}, chunks={tot}"
                        ))

                    if chunk_id not in chunks:
                        chunks[chunk_id] = payload

                    if total_chunks is not None and len(chunks) == total_chunks:
                        # reconstitution
                        ordered = b"".join(chunks[i] for i in range(total_chunks))
                        with open(OUT_IMAGE, "wb") as f:
                            f.write(ordered)
                        dt = time.time() - start
                        rx.close()

                        # mise à jour UI
                        def done():
                            self.path_var.set(os.path.abspath(OUT_IMAGE))
                            self.refresh_image()
                            self.set_status(f"Image reçue -> {OUT_IMAGE} ({len(ordered)} octets) en {dt:.2f}s")
                        self.root.after(0, done)
                        return

                rx.close()
                self.root.after(0, lambda: self.set_status("Timeout: image incomplète ou non reçue."))
            except Exception as e:
                rx.close()
                self.root.after(0, lambda: self.set_status(f"Erreur réception image: {e}"))

        threading.Thread(target=worker, daemon=True).start()


if __name__ == "__main__":
    root = tk.Tk()
    root.geometry("1000x700")
    app = MicroZedGUI(root)
    root.mainloop()
