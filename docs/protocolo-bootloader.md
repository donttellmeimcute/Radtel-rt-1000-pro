# Protocolo del bootloader (actualizacion de firmware)

Obtenido por ingenieria inversa del actualizador oficial `Radtel_RT-1000Pro_Update_260611_3.exe`
(aplicacion .NET; clase `Ido_Update.UpDate`, metodos `RevDateOperation`, `<RevDateOperation>b__0`,
`checksum`, `stringopration`). Implementado en `tools/flash.py`.

## Entrada al bootloader
1. Conectar el cable de programacion.
2. Mantener **PTT** presionado y encender el radio.

## Enlace
- 115200 baudios, 8N1.
- El radio responde un byte por cada paquete: `0x06` = ACK, `0xFF` = error.

## Checksum
`ck = (suma de todos los bytes del paquete excepto el ultimo + 0x52 + 0x54 + 1) mod 256`
(0x52 0x54 = "RT").

## Paquetes de control (5 bytes)
| Nombre | Bytes |
|---|---|
| CONNECT | `39 33 05 10 28` |
| UPDATE  | `39 33 05 55 6D` |
| END     | `39 33 05 EE 06` (definido, el actualizador no lo envia) |

## Paquete de datos (1028 bytes)
`57 | (offset>>8)&0xFF | offset&0xFF | 1024 bytes de datos | ck`

`offset` es la posicion dentro de la imagen (multiplo de 1024), por lo que el tercer byte siempre es 0
y el segundo da la vuelta cada 64 KB (el bootloader cuenta los bloques en orden).

## Secuencia
1. PC -> CONNECT, radio -> ACK (tres veces en total).
2. PC -> UPDATE, radio -> ACK.
3. 246 paquetes de datos (251904 bytes = 0x08002800..0x0803F7FF), esperando ACK antes de cada uno.
   El actualizador no espera el ACK del ultimo paquete.

La parte no usada de la imagen se rellena con 0x00. El bootloader (0x08000000-0x080027FF) y la ultima
pagina de 2 KB (0x0803F800) no se escriben.

## Formato del firmware dentro del .exe
El firmware va como un literal de texto Intel HEX (UTF-16LE) en el heap `#US` del ensamblado .NET,
**sin cifrar**. Registro inicial `:020000040800F2`, datos desde 0x08002800, entrada 0x080029E1.
`tools/extract_firmware.py` lo extrae.
