/* Linker script - Radtel RT-1000 Pro (AT32F435, 256 KB flash)
 * El bootloader de fabrica ocupa 0x08000000-0x080027FF y salta a 0x08002800.
 * El actualizador oficial escribe exactamente 246 KB (0x08002800-0x0803F7FF);
 * los ultimos 2 KB (0x0803F800) no se tocan.                                   */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08002800, LENGTH = 251904
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K    /* el original usa ~44 KB; 64 KB es seguro en todas las variantes */
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : {
    *(.text*) *(.rodata*)
    . = ALIGN(4);
  } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  _sidata = LOADADDR(.data);
  .data : {
    _sdata = .; *(.data*); . = ALIGN(4); _edata = .;
  } > RAM AT > FLASH
  .bss (NOLOAD) : {
    _sbss = .; *(.bss*) *(COMMON); . = ALIGN(4); _ebss = .;
    end = .; _end = .;   /* inicio del heap (newlib) */
  } > RAM
  /DISCARD/ : { *(.ARM.attributes) }
}
