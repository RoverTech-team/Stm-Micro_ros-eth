/*
******************************************************************************
**  LinkerScript.ld — STM32H755 CM4 core
**  Flash: 0x08100000 (bank 2), 1MB
**  RAM:   0x10000000 (SRAM3 D2 domain), 288KB
**
**  Fixes vs original:
**    1. _edata moved to end of .data (not inside .tdata) — fixes startup copy range
**    2. Removed /DISCARD/ libgcc/libc/libm — prevents silent loss of __aeabi_* helpers
**    3. Merged .tdata into .data — no TLS needed for bare-metal HAL project
**    4. Cleaned up redundant PROVIDE symbols for TLS (unused in this project)
******************************************************************************
*/

ENTRY(Reset_Handler)

/* ── Memory map ──────────────────────────────────────────────────────────── */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08100000, LENGTH = 1024K   /* CM4 flash bank 2   */
  RAM   (xrw) : ORIGIN = 0x10000000, LENGTH = 288K    /* SRAM3 D2 domain    */
}

_estack        = ORIGIN(RAM) + LENGTH(RAM);  /* 0x10048000 — top of stack   */
_Min_Heap_Size  = 0x400;                     /* 1KB heap                    */
_Min_Stack_Size = 0x800;                     /* 2KB stack (increased)       */

/* ── Output sections ─────────────────────────────────────────────────────── */
SECTIONS
{
  /* ------------------------------------------------------------------ */
  /* Vector table — must be the very first thing at 0x08100000          */
  /* ------------------------------------------------------------------ */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ------------------------------------------------------------------ */
  /* Code                                                                */
  /* ------------------------------------------------------------------ */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* ------------------------------------------------------------------ */
  /* Read-only data                                                      */
  /* ------------------------------------------------------------------ */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ------------------------------------------------------------------ */
  /* C++ exception index tables (required even if not using C++)         */
  /* ------------------------------------------------------------------ */
  .ARM.extab (READONLY) :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM (READONLY) :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* ------------------------------------------------------------------ */
  /* C init/fini arrays                                                  */
  /* ------------------------------------------------------------------ */
  .preinit_array (READONLY) :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array (READONLY) :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array (READONLY) :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* ------------------------------------------------------------------ */
  /* Initialized data — loaded from flash, copied to RAM by startup.s   */
  /*                                                                     */
  /* FIX 1: _edata is now at the end of .data, not buried in .tdata.    */
  /* The startup loop:  for(p=_sdata; p<_edata; ) *p++ = *q++;          */
  /* must span the exact same range as the AT>FLASH load region.        */
  /* ------------------------------------------------------------------ */
  _sidata = LOADADDR(.data);   /* LMA — source address in flash          */

  .data :
  {
    . = ALIGN(4);
    _sdata = .;          /* VMA start — destination in RAM               */
    *(.data)
    *(.data*)
    *(.tdata)            /* merge thread-local data here (bare-metal)    */
    *(.tdata*)
    . = ALIGN(4);
    _edata = .;          /* VMA end — copy stops here                    */
  } >RAM AT> FLASH

  /* ------------------------------------------------------------------ */
  /* Uninitialized data — zeroed by startup.s                           */
  /* ------------------------------------------------------------------ */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(.tbss)             /* merge thread-local BSS here (bare-metal)     */
    *(.tbss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* ------------------------------------------------------------------ */
  /* Heap + stack space sanity check                                     */
  /* ------------------------------------------------------------------ */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end  = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* ------------------------------------------------------------------ */
  /* FIX 2: /DISCARD/ block REMOVED.                                    */
  /*                                                                     */
  /* Discarding libgcc silently removes __aeabi_uidiv, __aeabi_idiv and */
  /* all soft-float helpers. Any HAL call that uses integer division     */
  /* (e.g. prescaler calculations) would jump to address 0x0 instead,   */
  /* immediately causing a HardFault with no obvious cause.             */
  /*                                                                     */
  /* Dead-code elimination is handled by the linker flags:              */
  /*   -Wl,--gc-sections  +  -ffunction-sections  -fdata-sections       */
  /* Add these to CFLAGS and LDFLAGS in your Makefile instead.          */
  /* ------------------------------------------------------------------ */

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
