#!/usr/bin/env python3
"""Flasher libre para el bootloader del Radtel RT-1000 Pro.

Protocolo obtenido por ingenieria inversa del actualizador oficial (ver
docs/protocolo-bootloader.md). Requiere pyserial:  pip install pyserial

Pasos:
  1. Conecta el cable de programacion.
  2. Manten presionado PTT y enciende el radio (entra al bootloader).
  3. python3 flash.py -p /dev/ttyUSB0 firmware.bin

Acepta .bin (imagen plana que empieza en 0x08002800) o .hex (Intel HEX).

AVISO: flashear firmware no oficial puede dejar el radio inservible y anular la
garantia. El bootloader (0x08000000-0x080027FF) no se toca, por lo que siempre
deberia ser posible volver a flashear el firmware original.
"""
import argparse
import sys
import time

APP_BASE = 0x08002800
APP_SIZE = 251904          # 246 bloques * 1024
BLOCK = 1024
ACK, NAK = 0x06, 0xFF



def checksum(pkt: bytes) -> int:
    """Suma de todos los bytes menos el ultimo, + 'R' + 'T' + 1 (mod 256)."""
    return (sum(pkt[:-1]) + 0x52 + 0x54 + 1) & 0xFF


CMD_CONNECT = bytes([0x39, 0x33, 0x05, 0x10, 0x28])
CMD_UPDATE = bytes([0x39, 0x33, 0x05, 0x55, 0x6D])
CMD_END = bytes([0x39, 0x33, 0x05, 0xEE, 0x06])   # definido en el actualizador, no se usa


def load_image(path: str, pad: int) -> bytes:
    if path.lower().endswith(".hex"):
        sys.path.insert(0, __import__("os").path.dirname(__file__))
        from extract_firmware import hex_to_image
        text = open(path).read().replace("\r", "").replace("\n", "")
        lo, img, _ = hex_to_image(text)
        if lo != APP_BASE:
            raise SystemExit(f"El HEX empieza en 0x{lo:08X}, se esperaba 0x{APP_BASE:08X}")
    else:
        img = open(path, "rb").read()
    if len(img) > APP_SIZE:
        raise SystemExit(f"Imagen demasiado grande: {len(img)} > {APP_SIZE}")
    sp = int.from_bytes(img[0:4], "little")
    rst = int.from_bytes(img[4:8], "little")
    if not (0x20000000 <= sp <= 0x20060000) or not (APP_BASE <= (rst & ~1) < APP_BASE + APP_SIZE):
        raise SystemExit(f"La imagen no parece valida (SP=0x{sp:08X}, Reset=0x{rst:08X})")
    return img + bytes([pad]) * (APP_SIZE - len(img))


def data_packet(img: bytes, offset: int) -> bytes:
    pkt = bytearray(BLOCK + 4)
    pkt[0] = 0x57                       # 'W'
    pkt[1] = (offset >> 8) & 0xFF       # el actualizador manda offset>>8 truncado a 8 bits
    pkt[2] = offset & 0xFF
    pkt[3:3 + BLOCK] = img[offset:offset + BLOCK]
    pkt[-1] = checksum(pkt)
    return bytes(pkt)


def wait_ack(ser, what: str, timeout: float = 3.0):
    ser.timeout = timeout
    r = ser.read(1)
    if not r:
        raise SystemExit(f"Sin respuesta del radio ({what}). Esta en modo bootloader?")
    if r[0] == ACK:
        return
    if r[0] == NAK:
        raise SystemExit(f"El radio respondio error (0xFF) en {what}")
    raise SystemExit(f"Respuesta inesperada 0x{r[0]:02X} en {what}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image")
    ap.add_argument("-p", "--port", required=True)
    ap.add_argument("--pad", type=lambda x: int(x, 0), default=0x00,
                    help="byte de relleno hasta 251904 bytes (el oficial usa 0x00)")
    ap.add_argument("--dry-run", action="store_true", help="solo valida y muestra los paquetes")
    a = ap.parse_args()

    img = load_image(a.image, a.pad)
    nblocks = APP_SIZE // BLOCK
    if a.dry_run:
        print("CONNECT", CMD_CONNECT.hex(" "), "UPDATE", CMD_UPDATE.hex(" "))
        print("bloque 0:", data_packet(img, 0)[:8].hex(" "), "...", f"{data_packet(img, 0)[-1]:02x}")
        print(f"{nblocks} bloques de {BLOCK} bytes")
        return

    import serial  # pyserial
    with serial.Serial(a.port, 115200, bytesize=8, parity="N", stopbits=1) as ser:
        ser.reset_input_buffer()
        # El actualizador oficial envia CONNECT 3 veces (cada una tras un ACK)
        for i in range(3):
            ser.write(CMD_CONNECT)
            wait_ack(ser, f"CONNECT #{i + 1}")
            time.sleep(0.05)
        ser.write(CMD_UPDATE)
        wait_ack(ser, "UPDATE", timeout=10.0)   # el bootloader puede borrar flash aqui
        for n in range(nblocks):
            ser.write(data_packet(img, n * BLOCK))
            if n < nblocks - 1:
                wait_ack(ser, f"bloque {n}")
            print(f"\r{n + 1:03d}/{nblocks}", end="", flush=True)
            time.sleep(0.01)
        print("\nListo. Reinicia el radio.")


if __name__ == "__main__":
    main()
