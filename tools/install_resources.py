#!/usr/bin/env python3
"""Instala en la flash SPI los recursos que el firmware libre NO puede incluir
por derechos de autor, extrayendolos del actualizador oficial que tu ya tienes:

  * Parche SSB del Si4735 (Silicon Labs) -> 0x094000  (activa USB/LSB en el receptor)
  * Indice de los mensajes de voz         -> 0x09C000  (los audios ya estan en la flash del radio)

Uso (con el FIRMWARE LIBRE ya instalado y el radio encendido normalmente):
  python3 install_resources.py -p /dev/ttyUSB0 Radtel_RT-1000Pro_Update_260611_3.exe

Solo se ha verificado con la version 260611 del actualizador.
"""
import argparse
import os
import struct
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract_firmware import find_hex_text, hex_to_image, APP_BASE
from cps_link import CpsLink

SSB_PATCH_ADDR = 0x0803181A
SSB_PATCH_LEN = 0x7BB * 8          # 15832 bytes
VOICE_INDEX_ADDR = 0x080314F1
VOICE_COUNT = 57
FLASH_SSB = 0x094000
FLASH_VIDX = 0x09C000


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("updater", help="actualizador oficial .exe (o stock.bin ya extraido)")
    ap.add_argument("-p", "--port")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    raw = open(a.updater, "rb").read()
    if a.updater.lower().endswith(".exe"):
        base, img, _ = hex_to_image(find_hex_text(raw))
    else:
        base, img = APP_BASE, raw
    off = SSB_PATCH_ADDR - base
    patch = img[off:off + SSB_PATCH_LEN]
    if len(patch) != SSB_PATCH_LEN or patch[:4] != bytes([0x15, 0x00, 0x0F, 0xE0]):
        sys.exit("No se reconoce el parche SSB en esta version del firmware")
    ssb_blob = b"SSBP" + struct.pack("<I", len(patch)) + patch

    voff = VOICE_INDEX_ADDR - base
    entries = []
    for i in range(VOICE_COUNT):
        o, n = struct.unpack_from("<IH", img, voff + i * 6)
        entries.append((o, n))
    if not all(0 <= o < 0x100000 and 0 < n < 0x40000 for o, n in entries):
        sys.exit("El indice de voz no parece valido en esta version")
    vidx = b"VIDX" + struct.pack("<H", VOICE_COUNT) + b"".join(struct.pack("<II", o, n) for o, n in entries)

    print(f"Parche SSB: {len(patch)} bytes; indice de voz: {VOICE_COUNT} mensajes")
    if a.dry_run:
        return
    if not a.port:
        sys.exit("Falta -p PUERTO")
    link = CpsLink(a.port)
    try:
        link.enter()
        link.write_raw(FLASH_SSB, ssb_blob)
        link.write_raw(FLASH_VIDX, vidx)
        link.leave()
    finally:
        link.close()
    print("Recursos instalados.")


if __name__ == "__main__":
    main()
