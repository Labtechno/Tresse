// Démarrage minimal pour les programmes de test Cortex-M3.
#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern void __libc_init_array(void);
extern int main(void);
void finish(int code);

void Reset_Handler(void) {
  uint32_t* src = &_sidata;
  for (uint32_t* dst = &_sdata; dst < &_edata;) *dst++ = *src++;
  for (uint32_t* dst = &_sbss; dst < &_ebss;) *dst++ = 0;
  __libc_init_array();
  finish(main());
}

void Default_Handler(void) { finish(99); }
#define WEAK(name) void name(void) __attribute__((weak, alias("Default_Handler")))
WEAK(NMI_Handler); WEAK(HardFault_Handler); WEAK(SVC_Handler); WEAK(PendSV_Handler);
WEAK(SysTick_Handler); WEAK(IRQ0_Handler); WEAK(IRQ1_Handler); WEAK(IRQ2_Handler);

__attribute__((section(".vectors"), used)) void (*const vectors[])(void) = {
  (void (*)(void)) &_estack, Reset_Handler, NMI_Handler, HardFault_Handler,
  Default_Handler, Default_Handler, Default_Handler, 0, 0, 0, 0,
  SVC_Handler, Default_Handler, 0, PendSV_Handler, SysTick_Handler,
  IRQ0_Handler, IRQ1_Handler, IRQ2_Handler,
};

// Sortie : port série de test (UART0 de la carte lm3s6965evb sous QEMU).
void put(char c) { *(volatile uint32_t*) 0x4000c000 = (uint32_t) c; }

void emit(uint32_t v) {
  for (int i = 28; i >= 0; i -= 4) put("0123456789abcdef"[(v >> i) & 15]);
  put('\n');
}

void finish(int code) {
  *(volatile uint32_t*) 0x4000c004 = (uint32_t) code;   // fin pour l'émulateur Tresses
  register uint32_t r0 __asm__("r0") = 0x18;             // SYS_EXIT (semihosting QEMU)
  register uint32_t r1 __asm__("r1") = 0x20026;
  __asm__ volatile("bkpt 0xab" : : "r"(r0), "r"(r1));
  for (;;) {}
}

void* __dso_handle;
void _init(void) {}
void _fini(void) {}
void _exit(int code) { finish(code); for (;;) {} }
