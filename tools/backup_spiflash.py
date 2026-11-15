#!/usr/bin/env python3
"""Respaldo de la flash SPI externa del RT-1000 Pro por el cable de programacion.

HAZLO ANTES DE FLASHEAR CUALQUIER FIRMWARE: la flash contiene la calibracion de
fabrica (0x000000-0x001FFF), unica de cada radio.

  python3 backup_spiflash.py -p /dev/ttyUSB0                 # calibracion+ajustes+canales+zonas
  python3 backup_spiflash.py -p /dev/ttyUSB0 --full          # los 4 MB completos (~7 min)
  python3 backup_spiflash.py -p COM5 --start 0 --size 0x2000 -o cal.bin

Funciona con el firmware ORIGINAL encendido normalmente (no en modo bootloader).
"""
import argparse
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cps_link import CpsLink


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port", required=True)
    ap.add_argument("-o", "--out", default="spiflash_backup.bin")
    ap.add_argument("--full", action="store_true", help="leer los 4 MB completos")
    ap.add_argument("--start", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--size", type=lambda x: int(x, 0), default=0x5E000,
                    help="bytes a leer (default 0x5E000: calibracion, ajustes, canales, zonas)")
    a = ap.parse_args()
    if a.full:
        a.start, a.size = 0, 0x400000
    if a.start % 1024 or a.size % 1024:
        sys.exit("start y size deben ser multiplos de 1024")
    link = CpsLink(a.port)
    try:
        link.enter()
        out = bytearray()
        nb = a.size // 1024
        for i in range(nb):
            out += link.read_block(a.start // 1024 + i)
            print(f"\r{i + 1}/{nb}", end="", flush=True)
        print()
        try:
            link.leave()
        except IOError:
            pass
    finally:
        link.close()
    open(a.out, "wb").write(out)
    if a.start == 0:
        print("Calibracion:", "OK (magic 0x9A)" if out[0] == 0x9A else f"magic inesperado 0x{out[0]:02X}")
    print(f"Guardado {len(out)} bytes en {a.out}")


if __name__ == "__main__":
    main()
