/* Programme de test : flash à FLASH_ORIGIN (0 pour QEMU lm3s6965evb, 0x08000000 pour un STM32). */
MEMORY {
  FLASH (rx) : ORIGIN = FLASH_ORIGIN, LENGTH = 256K
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
ENTRY(Reset_Handler)
SECTIONS {
  .text : {
    KEEP(*(.vectors))
    *(.text*) *(.rodata*)
    . = ALIGN(4);
    KEEP(*(.init)) KEEP(*(.fini))
    __preinit_array_start = .; KEEP(*(.preinit_array)) __preinit_array_end = .;
    __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .;
    __fini_array_start = .; KEEP(*(.fini_array)) __fini_array_end = .;
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    __exidx_start = .; *(.ARM.exidx*) __exidx_end = .;
    . = ALIGN(4);
  } > FLASH
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) *(.ramtext*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; end = .; _end = .; } > RAM
}
