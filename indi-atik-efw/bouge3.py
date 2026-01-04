import usb.core
import usb.util
import time

def bouge_roue_pi5_total_reset(num_filtre):
    dev = usb.core.find(idVendor=0x0403, idProduct=0xaf01)
    if dev is None:
        print("Roue introuvable")
        return

    # --- ÉTAPE 1 : RESET MATÉRIEL DU PORT ---
    try:
        dev.reset() # Simule un débranchement/rebranchement
        time.sleep(1) # Crucial : laisser le temps au Pi de redécouvrir le périphérique
        # Après un reset, il faut retrouver l'objet 'dev' car l'ancien est invalide
        dev = usb.core.find(idVendor=0x0403, idProduct=0xaf01)
    except Exception as e:
        print(f"Note sur le reset : {e}")

    # --- ÉTAPE 2 : GESTION DU KERNEL ---
    if dev.is_kernel_driver_active(0):
        dev.detach_kernel_driver(0)

    dev.set_configuration()
    
    # --- ÉTAPE 3 : CONFIGURATION FTDI ---
    # Reset FTDI (0), Baudrate 9600 (3), Flow Control (1)
    dev.ctrl_transfer(0x40, 0, 0, 0, None)
    dev.ctrl_transfer(0x40, 3, 0x4138, 0, None)
    dev.ctrl_transfer(0x40, 1, 0x0303, 0, None)
    time.sleep(0.2)

    # --- ÉTAPE 4 : RÉVEIL ET ORDRE ---
    # On vide le buffer
    try: dev.read(0x81, 64, timeout=100)
    except: pass

    # On envoie d'abord un "Statut" pour réveiller
    dev.write(0x02, [0x23, 0x04, 0x00, 0x23])
    time.sleep(0.1)

    print(f"Envoi Filtre {num_filtre}...")
    dev.write(0x02, [0x23, 0x01, num_filtre, 0x23])
    
    # --- ÉTAPE 5 : ATTENTE ---
    time.sleep(1.5)
    try:
        res = dev.read(0x81, 64, timeout=1000)
        clean = [hex(x) for x in res if x not in [0x01, 0x60]]
        print(f"Réponse roue : {clean}")
    except:
        print("Pas de réponse lue")

    # Libération
    usb.util.dispose_resources(dev)

# Test
import sys
f = int(sys.argv[1]) if len(sys.argv) > 1 else 1
bouge_roue_pi5_total_reset(f)
