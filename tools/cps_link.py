"""Enlace serie con el protocolo de programacion del RT-1000 Pro (USART6,
115200 8N1). Funciona tanto con el firmware original como con el libre."""
import time

ACK = 0x06


def sum8(b: bytes) -> int:
    return sum(b) & 0xFF


class CpsLink:
    def __init__(self, port: str, timeout: float = 2.0):
        import serial  # pyserial
        self.ser = serial.Serial(port, 115200, bytesize=8, parity="N", stopbits=1, timeout=timeout)
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def _cmd34(self, sub: int):
        f = bytes([0x34, 0x00, 0x00, sub])
        self.ser.write(f + bytes([sum8(f)]))
        r = self.ser.read(1)
        if not r or r[0] != ACK:
            raise IOError(f"El radio no respondio al comando 0x34/{sub:02X} ({r!r})")

    def enter(self):
        self._cmd34(0x10)

    def leave(self):
        self._cmd34(0xEE)

    def read_block(self, blk: int) -> bytes:
        f = bytes([0x52, blk >> 8, blk & 0xFF])
        self.ser.write(f + bytes([sum8(f)]))
        r = self.ser.read(1028)
        if len(r) != 1028 or r[0] != 0x52 or r[1] != (blk >> 8) or r[2] != (blk & 0xFF):
            raise IOError(f"Respuesta invalida al leer el bloque {blk}")
        if sum8(r[:1027]) != r[1027]:
            raise IOError(f"Checksum invalido en el bloque {blk}")
        return r[3:1027]

    def write_block(self, cmd: int, blk: int, data: bytes):
        assert len(data) == 1024
        f = bytes([cmd, blk >> 8, blk & 0xFF]) + data
        self.ser.write(f + bytes([sum8(f)]))
        r = self.ser.read(1)
        if not r or r[0] != ACK:
            raise IOError(f"Escritura rechazada (cmd {cmd:02X} bloque {blk}): {r!r}")

    def write_raw(self, addr: int, data: bytes):
        """Escritura cruda con comando 0xA4 (borra el sector al escribir su primer bloque).
        addr debe estar alineada a 4 KB; se rellena con 0xFF hasta multiplos de 4 KB."""
        assert addr % 4096 == 0
        data = data + b"\xff" * ((-len(data)) % 4096)
        for i in range(0, len(data), 1024):
            self.write_block(0xA4, (addr + i) // 1024, data[i:i + 1024])
            time.sleep(0.005)
