// Test des exceptions : SysTick, interruptions imbriquées, PendSV, SVC, masquage.
#include <stdint.h>
void emit(uint32_t v);

#define REG(a) (*(volatile uint32_t*) (a))
#define SYST_CSR REG(0xe000e010)
#define SYST_RVR REG(0xe000e014)
#define SYST_CVR REG(0xe000e018)
#define NVIC_ISER REG(0xe000e100)
#define NVIC_ISPR REG(0xe000e200)
#define NVIC_IPR(n) (*(volatile uint8_t*) (0xe000e400 + (n)))
#define ICSR REG(0xe000ed04)
#define SHPR(n) (*(volatile uint8_t*) (0xe000ed18 + (n) - 4))

static volatile uint32_t ticks, depth, sum;

void SysTick_Handler(void) { ++ticks; }
void SVC_Handler(void) { emit(0x5c000000 | depth); }
void PendSV_Handler(void) { emit(0x9e000000 | depth); }
void IRQ2_Handler(void) { ++depth; emit(0x22000000 | depth); --depth; }
void IRQ1_Handler(void) {
  ++depth; emit(0x11000000 | depth);
  NVIC_ISPR = 4;            // IRQ2, plus prioritaire : préemption immédiate
  emit(0x11100000 | depth);
  ICSR = 1u << 28;          // PendSV, moins prioritaire : après la sortie
  emit(0x11200000 | depth); --depth;
}
void IRQ0_Handler(void) {
  ++depth; emit(0x00a00000 | depth);
  NVIC_ISPR = 2;            // IRQ1, plus prioritaire
  emit(0x00b00000 | depth);
  __asm__ volatile("svc 5");
  emit(0x00c00000 | depth); --depth;
}

__attribute__((noinline)) static uint32_t work(uint32_t n) {
  uint32_t a = 1;
  for (uint32_t i = 0; i < n; ++i) { a = a * 1664525u + 1013904223u; sum += a >> 7; }
  return a;
}

int main(void) {
  NVIC_IPR(0) = 0xc0; NVIC_IPR(1) = 0x80; NVIC_IPR(2) = 0x40;
  SHPR(11) = 0x40; SHPR(14) = 0xe0; SHPR(15) = 0x20;
  NVIC_ISER = 7;

  emit(1);
  NVIC_ISPR = 1;                              // lance la cascade IRQ0 > IRQ1 > IRQ2 > PendSV
  emit(2);

  __asm__ volatile("cpsid i");                // PRIMASK : rien ne passe
  NVIC_ISPR = 4; emit(3);
  __asm__ volatile("cpsie i"); emit(4);

  uint32_t base = 0x80;                       // BASEPRI = 0x80 : seul IRQ2 (0x40) passe
  __asm__ volatile("msr basepri, %0" : : "r"(base));
  NVIC_ISPR = 2; NVIC_ISPR = 4; emit(5);
  base = 0; __asm__ volatile("msr basepri, %0" : : "r"(base)); emit(6);

  // SysTick : les calculs en cours ne doivent pas être perturbés par l'interruption.
  uint32_t before = work(2000);
  SYST_RVR = 999; SYST_CVR = 0; SYST_CSR = 7;
  uint32_t during = work(2000);
  while (ticks < 50) {}
  uint32_t t1 = ticks;
  for (int i = 0; i < 10; ++i) __asm__ volatile("wfi");
  SYST_CSR = 0;
  emit(before == during); emit(t1 >= 50); emit(ticks - t1 >= 9 && ticks - t1 <= 11);

  // Pile : les registres r4-r11 et les drapeaux survivent aux interruptions.
  SYST_CSR = 7;
  uint32_t ok = 1;
  for (uint32_t i = 0; i < 20000; ++i) {
    uint64_t x = (uint64_t) i * 0x9e3779b97f4a7c15ull;
    uint64_t y = x + 0xffffffffull;            // retenue qui traverse une interruption
    if (y - 0xffffffffull != x) ok = 0;
  }
  SYST_CSR = 0;
  emit(ok); emit(ticks >= 60);
  return 0;
}
