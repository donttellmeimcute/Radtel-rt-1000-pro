#!/usr/bin/env python3
"""Extrae el firmware del actualizador oficial de Radtel (RT-1000 Pro).

El actualizador (Radtel_RT-1000Pro_Update_XXXXXX.exe) es una aplicacion .NET que
lleva el firmware incrustado como un literal de texto Intel HEX (UTF-16LE) dentro
del heap #US. El firmware NO esta cifrado.

Uso:
    python3 extract_firmware.py Radtel_RT-1000Pro_Update_260611_3.exe -o stock

Genera stock.hex (Intel HEX original) y stock.bin (imagen plana desde 0x08002800).
Sin dependencias externas.
"""
import argparse
import sys

APP_BASE = 0x08002800
APP_SIZE = 251904  # 246 bloques de 1024 bytes, igual que el actualizador


def find_hex_text(exe: bytes) -> str:
    start_marker = ":020000040800F2".encode("utf-16le")
    start = exe.find(start_marker)
    if start < 0:
        raise SystemExit("No se encontro el Intel HEX incrustado en el ejecutable")
    # Leer caracteres UTF-16LE mientras sean ':' o hexadecimales
    allowed = set(b":0123456789ABCDEFabcdef")
    end = start
    while end + 1 < len(exe) and exe[end + 1] == 0 and exe[end] in allowed:
        end += 2
    return exe[start:end].decode("utf-16le")


def hex_to_image(text: str):
    mem = {}
    base = 0
    start_addr = None
    for rec in text.split(":")[1:]:
        raw = bytes.fromhex(rec)
        if sum(raw) & 0xFF:
            raise ValueError("checksum Intel HEX invalido en registro :" + rec)
        n, addr, rtype, data = raw[0], (raw[1] << 8) | raw[2], raw[3], raw[4:4 + raw[0]]
        if rtype == 0:
            for i, b in enumerate(data):
                mem[base + addr + i] = b
        elif rtype == 4:
            base = int.from_bytes(data, "big") << 16
        elif rtype == 5:
            start_addr = int.from_bytes(data, "big")
        elif rtype == 1:
            break
    lo, hi = min(mem), max(mem) + 1
    img = bytearray(b"\xff" * (hi - lo))
    for a, b in mem.items():
        img[a - lo] = b
    return lo, bytes(img), start_addr


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe")
    ap.add_argument("-o", "--out", default="stock", help="prefijo de salida (default: stock)")
    a = ap.parse_args()
    exe = open(a.exe, "rb").read()
    text = find_hex_text(exe)
    lo, img, start = hex_to_image(text)
    if lo != APP_BASE:
        print(f"Aviso: base inesperada 0x{lo:08X}", file=sys.stderr)
    # El HEX lo escribimos con un registro por linea (el original va en una sola linea)
    with open(a.out + ".hex", "w") as f:
        f.write("\n".join(":" + r for r in text.split(":")[1:]) + "\n")
    with open(a.out + ".bin", "wb") as f:
        f.write(img)
    sp = int.from_bytes(img[0:4], "little")
    rst = int.from_bytes(img[4:8], "little")
    print(f"Imagen: 0x{lo:08X}-0x{lo + len(img):08X} ({len(img)} bytes, max {APP_SIZE})")
    print(f"SP inicial 0x{sp:08X}, Reset_Handler 0x{rst:08X}, entry HEX {start and hex(start)}")
    print(f"Escrito {a.out}.hex y {a.out}.bin")


if __name__ == "__main__":
    main()
