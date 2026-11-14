#!/usr/bin/env python3
# make_flh.py — ESP32-CoCo3-PRO : image .bin compilée -> fichier .FLH
# Format : <somme décimale>-<texte>~<image> ; somme = octets à partir du « - » inclus (32 bits).
#   python make_flh.py ESP32-CoCo3-PRO.ino.bin ESP32-CoCo3-PRO_v1.0.FLH --text "ESP32-CoCo3-PRO v1.0"
import argparse, sys

ap = argparse.ArgumentParser(description="ESP32-CoCo3-PRO: .bin -> .FLH")
ap.add_argument("bin"); ap.add_argument("flh")
ap.add_argument("--text", default="ESP32-CoCo3-PRO firmware")
a = ap.parse_args()
img = open(a.bin, "rb").read()
if not img or img[0] != 0xE9:
    sys.exit("not an ESP32 application image (first byte must be 0xE9)")
if "~" in a.text:
    sys.exit("the text must not contain '~'")
body = b"-" + a.text.encode("ascii") + b"~" + img
s = sum(body) & 0xFFFFFFFF
open(a.flh, "wb").write(str(s).encode("ascii") + body)
print(f"{a.flh}: {len(img)} bytes of firmware, checksum {s}")
