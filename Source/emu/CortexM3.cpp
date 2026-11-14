// Tresses — émulateur ARM Cortex-M3 (voir CortexM3.h). Licence MIT.
//
// Références : ARMv7-M Architecture Reference Manual (décodage et pseudo-code des
// instructions, modèle d'exceptions) et Cortex-M3 Technical Reference Manual
// (durée des instructions en cycles).
//
// Durées utilisées (cycles d'horloge) :
//   opération sur registres ........ 1        chargement / rangement ...... 2
//   branchement pris ............... 3        LDM/STM/PUSH/POP ............ 1 + N
//   MUL ............................ 1        MLA / MLS ................... 2
//   multiplication 64 bits ......... 4 à 5    division .................... 2 à 12
//   entrée / sortie d'interruption . 12
// Les temps d'attente de la mémoire flash ne sont pas encore modélisés.

#include "CortexM3.h"

#include <algorithm>

namespace tresses {
namespace emu {

namespace {

inline uint32_t ror32(uint32_t x, uint32_t n) {
  n &= 31;
  return n ? (x >> n) | (x << (32 - n)) : x;
}

inline int32_t signExtend(uint32_t value, int bits) {
  const uint32_t m = uint32_t { 1 } << (bits - 1);
  value &= (uint32_t { 1 } << bits) - 1;
  return static_cast<int32_t>((value ^ m) - m);
}

inline int popcount16(uint32_t x) {
  int n = 0;
  for (x &= 0xffff; x; x &= x - 1) ++n;
  return n;
}

inline int clz32(uint32_t x) {
  if (x == 0) return 32;
  int n = 0;
  while (!(x & 0x80000000u)) { x <<= 1; ++n; }
  return n;
}

enum ShiftType { kLSL = 0, kLSR = 1, kASR = 2, kROR = 3, kRRX = 4 };

// Shift_C du manuel ARM. n = 0 : valeur et retenue inchangées.
inline uint32_t shiftC(uint32_t x, int type, uint32_t n, bool& carry) {
  if (n == 0) return x;
  switch (type) {
    case kLSL:
      if (n < 32) { carry = (x >> (32 - n)) & 1; return x << n; }
      carry = n == 32 ? (x & 1) : false;
      return 0;
    case kLSR:
      if (n < 32) { carry = (x >> (n - 1)) & 1; return x >> n; }
      carry = n == 32 ? (x >> 31) : false;
      return 0;
    case kASR:
      if (n < 32) {
        carry = (x >> (n - 1)) & 1;
        return static_cast<uint32_t>(static_cast<int32_t>(x) >> n);
      }
      carry = x >> 31;
      return (x >> 31) ? 0xffffffffu : 0;
    case kROR: {
      const uint32_t result = ror32(x, n);
      carry = result >> 31;
      return result;
    }
    default: {  // RRX
      const uint32_t result = (static_cast<uint32_t>(carry) << 31) | (x >> 1);
      carry = x & 1;
      return result;
    }
  }
}

// DecodeImmShift + Shift_C pour un décalage immédiat sur 5 bits.
inline uint32_t shiftImm(uint32_t x, uint32_t type, uint32_t imm5, bool& carry) {
  switch (type) {
    case 0: return shiftC(x, kLSL, imm5, carry);
    case 1: return shiftC(x, kLSR, imm5 ? imm5 : 32, carry);
    case 2: return shiftC(x, kASR, imm5 ? imm5 : 32, carry);
    default:
      return imm5 ? shiftC(x, kROR, imm5, carry) : shiftC(x, kRRX, 1, carry);
  }
}

inline uint32_t thumbExpandImm(uint32_t imm12, bool& carry) {
  if ((imm12 & 0xc00) == 0) {
    const uint32_t imm8 = imm12 & 0xff;
    switch ((imm12 >> 8) & 3) {
      case 0: return imm8;
      case 1: return (imm8 << 16) | imm8;
      case 2: return (imm8 << 24) | (imm8 << 8);
      default: return (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | imm8;
    }
  }
  const uint32_t unrotated = 0x80 | (imm12 & 0x7f);
  const uint32_t result = ror32(unrotated, imm12 >> 7);
  carry = result >> 31;
  return result;
}

}  // namespace

CortexM3::CortexM3(size_t flashSize, size_t ramSize, Bus* bus)
    : flash_(flashSize + 8, 0xff), ram_(ramSize + 8, 0),
      flashSize_(flashSize), ramSize_(ramSize), bus_(bus) {}

// --- Mémoire -----------------------------------------------------------------------

uint32_t CortexM3::read32(uint32_t a) {
  uint32_t o = a - kRamBase;
  if (o <= ramSize_ - 4) { uint32_t v; std::memcpy(&v, &ram_[o], 4); return v; }
  o = a - kFlashBase;
  if (o <= flashSize_ - 4) { uint32_t v; std::memcpy(&v, &flash_[o], 4); return v; }
  return slowRead(a, 4);
}

uint32_t CortexM3::read16(uint32_t a) {
  uint32_t o = a - kRamBase;
  if (o <= ramSize_ - 2) { uint16_t v; std::memcpy(&v, &ram_[o], 2); return v; }
  o = a - kFlashBase;
  if (o <= flashSize_ - 2) { uint16_t v; std::memcpy(&v, &flash_[o], 2); return v; }
  return slowRead(a, 2);
}

uint32_t CortexM3::read8(uint32_t a) {
  uint32_t o = a - kRamBase;
  if (o < ramSize_) return ram_[o];
  o = a - kFlashBase;
  if (o < flashSize_) return flash_[o];
  return slowRead(a, 1);
}

void CortexM3::write32(uint32_t a, uint32_t v) {
  const uint32_t o = a - kRamBase;
  if (o <= ramSize_ - 4) { std::memcpy(&ram_[o], &v, 4); return; }
  slowWrite(a, v, 4);
}

void CortexM3::write16(uint32_t a, uint32_t v) {
  const uint32_t o = a - kRamBase;
  if (o <= ramSize_ - 2) { const uint16_t h = static_cast<uint16_t>(v); std::memcpy(&ram_[o], &h, 2); return; }
  slowWrite(a, v & 0xffff, 2);
}

void CortexM3::write8(uint32_t a, uint32_t v) {
  const uint32_t o = a - kRamBase;
  if (o < ramSize_) { ram_[o] = static_cast<uint8_t>(v); return; }
  slowWrite(a, v & 0xff, 1);
}

uint32_t CortexM3::slowRead(uint32_t a, int size) {
  // Au démarrage, la flash est aussi visible à l'adresse 0.
  if (a < flashSize_ && a + size <= flashSize_) {
    uint32_t v = 0;
    std::memcpy(&v, &flash_[a], static_cast<size_t>(size));
    return v;
  }
  if ((a & 0xfffff000u) == 0xe000e000u) {
    const uint32_t word = scsRead(a & ~3u);
    const uint32_t shifted = word >> (8 * (a & 3));
    return size == 4 ? shifted : (size == 2 ? shifted & 0xffff : shifted & 0xff);
  }
  // Bit-banding : chaque bit de la RAM / des périphériques a son propre mot.
  if ((a & 0xfe000000u) == 0x22000000u || (a & 0xfe000000u) == 0x42000000u) {
    const uint32_t offset = a & 0x01ffffffu;
    const uint32_t target = (a & 0xf0000000u) + ((offset >> 5) & ~3u);
    const uint32_t bit = (((offset >> 5) & 3) << 3) + ((offset >> 2) & 7);
    return (read32(target) >> bit) & 1;
  }
  return bus_ ? bus_->read(a, size) : 0;
}

void CortexM3::slowWrite(uint32_t a, uint32_t v, int size) {
  if ((a & 0xfffff000u) == 0xe000e000u) {
    if (size == 4) { scsWrite(a, v); return; }
    // Registres de priorité : accès par octet.
    if ((a >= 0xe000e400u && a < 0xe000e400u + kNumIrqs) ||
        (a >= 0xe000ed18u && a < 0xe000ed24u)) {
      for (int i = 0; i < size; ++i) {
        const uint32_t addr = a + static_cast<uint32_t>(i);
        const int number = addr >= 0xe000ed18u ? static_cast<int>(addr - 0xe000ed18u) + 4
                                               : static_cast<int>(addr - 0xe000e400u) + 16;
        priority_[number] = static_cast<uint8_t>((v >> (8 * i)) & 0xf0);
      }
      updateExecutionPriority();
      irqDirty_ = true;
      return;
    }
    scsWrite(a & ~3u, v << (8 * (a & 3)));
    return;
  }
  if ((a & 0xfe000000u) == 0x22000000u || (a & 0xfe000000u) == 0x42000000u) {
    const uint32_t offset = a & 0x01ffffffu;
    const uint32_t target = (a & 0xf0000000u) + ((offset >> 5) & ~3u);
    const uint32_t bit = (((offset >> 5) & 3) << 3) + ((offset >> 2) & 7);
    const uint32_t word = read32(target);
    write32(target, (v & 1) ? (word | (1u << bit)) : (word & ~(1u << bit)));
    return;
  }
  if (bus_) bus_->write(a, v, size);
}

// --- Bloc de contrôle système (SysTick, NVIC, SCB) --------------------------------------

uint32_t CortexM3::scsRead(uint32_t a) {
  if (a >= 0xe000e100u && a < 0xe000e400u) {
    const uint32_t group = (a - 0xe000e100u) >> 7;   // ISER, ICER, ISPR, ICPR, IABR
    const int base = 16 + static_cast<int>(((a & 0x7f) >> 2) * 32);
    uint32_t v = 0;
    for (int i = 0; i < 32 && base + i < kNumExceptions; ++i) {
      const bool bit = group <= 1 ? enabled_[base + i]
                     : group <= 3 ? pending_[base + i] : active_[base + i];
      if (bit) v |= 1u << i;
    }
    return v;
  }
  if (a >= 0xe000e400u && a < 0xe000e400u + kNumIrqs) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      const int number = 16 + static_cast<int>(a - 0xe000e400u) + i;
      if (number < kNumExceptions) v |= static_cast<uint32_t>(priority_[number]) << (8 * i);
    }
    return v;
  }
  switch (a) {
    case 0xe000e010u: {
      // Lire le registre efface COUNTFLAG.
      const uint32_t v = systickCtrl_;
      systickCtrl_ &= ~0x10000u;
      return v;
    }
    case 0xe000e014u: return systickLoad_;
    case 0xe000e018u: return systickCurrent();
    case 0xe000e01cu: return 9000;                 // calibration 1 ms à 9 MHz (STM32)
    case 0xe000ed00u: return 0x411fc231u;          // Cortex-M3 r1p1
    case 0xe000ed04u: {
      uint32_t v = ipsr_;
      int best = 0;
      for (int i = 1; i < kNumExceptions; ++i) {
        if (pending_[i] && (best == 0 || groupPriority(i) < groupPriority(best))) best = i;
      }
      v |= static_cast<uint32_t>(best) << 12;
      if (best >= 16) v |= 1u << 22;
      if (pending_[15]) v |= 1u << 26;
      if (pending_[14]) v |= 1u << 28;
      if (pending_[2]) v |= 1u << 31;
      return v;
    }
    case 0xe000ed08u: return vtor_;
    case 0xe000ed0cu: return 0xfa050000u | (aircrPrigroup_ << 8);
    case 0xe000ed10u: return 0;
    case 0xe000ed14u: return ccr_;
    case 0xe000ed18u: case 0xe000ed1cu: case 0xe000ed20u: {
      uint32_t v = 0;
      for (int i = 0; i < 4; ++i) {
        v |= static_cast<uint32_t>(priority_[4 + (a - 0xe000ed18u) + i]) << (8 * i);
      }
      return v;
    }
    case 0xe000ed24u: return shcsr_;
    case 0xe000ed28u: return cfsr_;
    case 0xe000ed2cu: return hfsr_;
    default: return 0;
  }
}

void CortexM3::scsWrite(uint32_t a, uint32_t v) {
  if (a >= 0xe000e100u && a < 0xe000e300u) {
    const uint32_t group = (a - 0xe000e100u) >> 7;   // 0 ISER, 1 ICER, 2 ISPR, 3 ICPR
    const int base = 16 + static_cast<int>(((a & 0x7f) >> 2) * 32);
    for (int i = 0; i < 32 && base + i < kNumExceptions; ++i) {
      if (!(v & (1u << i))) continue;
      switch (group) {
        case 0: enabled_[base + i] = true; break;
        case 1: enabled_[base + i] = false; break;
        case 2: pending_[base + i] = true; break;
        default: pending_[base + i] = false; break;
      }
    }
    irqDirty_ = true;
    return;
  }
  if (a >= 0xe000e400u && a < 0xe000e400u + kNumIrqs) {
    for (int i = 0; i < 4; ++i) {
      const int number = 16 + static_cast<int>(a - 0xe000e400u) + i;
      if (number < kNumExceptions) priority_[number] = static_cast<uint8_t>((v >> (8 * i)) & 0xf0);
    }
    updateExecutionPriority();
    irqDirty_ = true;
    return;
  }
  switch (a) {
    case 0xe000e010u: {
      const uint32_t current = systickCurrent();
      systickValueAtRef_ = current;
      systickRefCycle_ = cycles_;
      systickCtrl_ = (systickCtrl_ & 0x10000u) | (v & 7);
      systickReschedule();
      break;
    }
    case 0xe000e014u:
      systickValueAtRef_ = systickCurrent();
      systickRefCycle_ = cycles_;
      systickLoad_ = v & 0x00ffffffu;
      systickReschedule();
      break;
    case 0xe000e018u:
      systickValueAtRef_ = 0;
      systickRefCycle_ = cycles_;
      systickCtrl_ &= ~0x10000u;
      systickReschedule();
      break;
    case 0xe000ed04u:
      if (v & (1u << 31)) pending_[2] = true;
      if (v & (1u << 28)) pending_[14] = true;
      if (v & (1u << 27)) pending_[14] = false;
      if (v & (1u << 26)) pending_[15] = true;
      if (v & (1u << 25)) pending_[15] = false;
      irqDirty_ = true;
      break;
    case 0xe000ed08u: vtor_ = v & 0x3fffff80u; break;
    case 0xe000ed0cu:
      if ((v >> 16) == 0x05fa) {
        aircrPrigroup_ = (v >> 8) & 7;
        updateExecutionPriority();
        irqDirty_ = true;
        if (v & 4) { reset(); }
      }
      break;
    case 0xe000ed14u: ccr_ = v; break;
    case 0xe000ed18u: case 0xe000ed1cu: case 0xe000ed20u:
      for (int i = 0; i < 4; ++i) {
        priority_[4 + (a - 0xe000ed18u) + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xf0);
      }
      updateExecutionPriority();
      irqDirty_ = true;
      break;
    case 0xe000ed24u: shcsr_ = v; break;
    case 0xe000ed28u: cfsr_ &= ~v; break;
    case 0xe000ed2cu: hfsr_ &= ~v; break;
    default: break;
  }
}

// --- SysTick -------------------------------------------------------------------------

uint32_t CortexM3::systickCurrent() const {
  if (!(systickCtrl_ & 1)) return systickValueAtRef_;
  const uint64_t divider = (systickCtrl_ & 4) ? 1 : 8;
  const uint64_t elapsed = (cycles_ - systickRefCycle_) / divider;
  if (elapsed <= systickValueAtRef_) return systickValueAtRef_ - static_cast<uint32_t>(elapsed);
  const uint64_t period = static_cast<uint64_t>(systickLoad_) + 1;
  const uint64_t rem = (elapsed - systickValueAtRef_ - 1) % period;
  return systickLoad_ - static_cast<uint32_t>(rem);
}

void CortexM3::systickReschedule() {
  systickNextFire_ = ~uint64_t { 0 };
  if ((systickCtrl_ & 1) && (systickLoad_ != 0 || systickValueAtRef_ != 0)) {
    const uint64_t divider = (systickCtrl_ & 4) ? 1 : 8;
    const uint64_t ticks = systickValueAtRef_ ? systickValueAtRef_
                                              : static_cast<uint64_t>(systickLoad_) + 1;
    systickNextFire_ = systickRefCycle_ + divider * ticks;
  }
  updateNextEvent();
}

void CortexM3::setExternalDeadline(uint64_t cycle) {
  externalDeadline_ = cycle;
  updateNextEvent();
}

void CortexM3::updateNextEvent() {
  nextEvent_ = std::min(systickNextFire_, externalDeadline_);
}

void CortexM3::serviceDeadlines() {
  while (cycles_ >= systickNextFire_) {
    systickCtrl_ |= 0x10000u;
    if (systickCtrl_ & 2) pendException(15);
    systickRefCycle_ = systickNextFire_;
    systickValueAtRef_ = 0;
    if (systickLoad_ == 0) { systickNextFire_ = ~uint64_t { 0 }; break; }
    const uint64_t divider = (systickCtrl_ & 4) ? 1 : 8;
    systickNextFire_ = systickRefCycle_ + divider * (static_cast<uint64_t>(systickLoad_) + 1);
  }
  if (cycles_ >= externalDeadline_) {
    externalDeadline_ = ~uint64_t { 0 };
    if (bus_) bus_->onDeadline(cycles_);
  }
  updateNextEvent();
}

// --- Exceptions ------------------------------------------------------------------------

void CortexM3::setIrqPending(int irq, bool pending) {
  if (irq < 0 || irq >= kNumIrqs) return;
  pending_[16 + irq] = pending;
  irqDirty_ = true;
}

int CortexM3::groupPriority(int number) const {
  if (number == 1) return -3;
  if (number == 2) return -2;
  if (number == 3) return -1;
  const uint32_t mask = ~((2u << aircrPrigroup_) - 1) & 0xff;
  return static_cast<int>(priority_[number] & mask);
}

void CortexM3::updateExecutionPriority() {
  int p = 256;
  for (int i = 1; i < kNumExceptions; ++i) {
    if (active_[i]) p = std::min(p, groupPriority(i));
  }
  if (basepri_) {
    const uint32_t mask = ~((2u << aircrPrigroup_) - 1) & 0xff;
    p = std::min(p, static_cast<int>(basepri_ & mask));
  }
  if (primask_) p = std::min(p, 0);
  if (faultmask_) p = std::min(p, -1);
  execPriority_ = p;
}

void CortexM3::checkInterrupts() {
  irqDirty_ = false;
  int best = 0;
  int bestPriority = 256;
  bool anyPending = false;
  for (int i = 2; i < kNumExceptions; ++i) {
    if (!pending_[i]) continue;
    if (i >= 16 && !enabled_[i]) continue;
    anyPending = true;
    const int p = groupPriority(i);
    if (p < bestPriority) { bestPriority = p; best = i; }
  }
  if (anyPending) sleeping_ = false;
  if (best != 0 && bestPriority < execPriority_) {
    exceptionEntry(best, pc_);
    pc_ = nextPc_;
  }
}

void CortexM3::switchStack(bool usePsp) {
  if (usePsp != usingPsp_) {
    std::swap(r_[13], otherSp_);
    usingPsp_ = usePsp;
  }
}

uint32_t CortexM3::xpsr() const {
  return (static_cast<uint32_t>(n_) << 31) | (static_cast<uint32_t>(z_) << 30) |
         (static_cast<uint32_t>(c_) << 29) | (static_cast<uint32_t>(v_) << 28) |
         (static_cast<uint32_t>(q_) << 27) | ((itstate_ & 3) << 25) | (1u << 24) |
         ((itstate_ >> 2) << 10) | ipsr_;
}

void CortexM3::exceptionEntry(int number, uint32_t returnAddress) {
  uint32_t sp = r_[13];
  uint32_t align = 0;
  if ((ccr_ & 0x200) && (sp & 4)) { sp -= 4; align = 1; }
  sp -= 32;
  write32(sp, r_[0]);
  write32(sp + 4, r_[1]);
  write32(sp + 8, r_[2]);
  write32(sp + 12, r_[3]);
  write32(sp + 16, r_[12]);
  write32(sp + 20, r_[14]);
  write32(sp + 24, returnAddress);
  write32(sp + 28, xpsr() | (align << 9));
  r_[13] = sp;

  r_[14] = ipsr_ != 0 ? 0xfffffff1u : (usingPsp_ ? 0xfffffffdu : 0xfffffff9u);
  switchStack(false);
  ipsr_ = static_cast<uint32_t>(number);
  itstate_ = 0;
  active_[number] = true;
  pending_[number] = false;
  updateExecutionPriority();
  nextPc_ = read32(vtor_ + 4 * static_cast<uint32_t>(number)) & ~1u;
  cycles_ += 12;
  irqDirty_ = true;
}

void CortexM3::exceptionReturn(uint32_t excReturn) {
  active_[ipsr_] = false;
  switch (excReturn & 0xf) {
    case 0x1: switchStack(false); break;
    case 0x9: switchStack(false); break;
    case 0xd: switchStack(true); break;
    default: halt("EXC_RETURN invalide"); return;
  }
  const bool toThread = (excReturn & 0xf) != 0x1;
  const uint32_t sp = r_[13];
  r_[0] = read32(sp);
  r_[1] = read32(sp + 4);
  r_[2] = read32(sp + 8);
  r_[3] = read32(sp + 12);
  r_[12] = read32(sp + 16);
  r_[14] = read32(sp + 20);
  nextPc_ = read32(sp + 24) & ~1u;
  const uint32_t psr = read32(sp + 28);
  r_[13] = (sp + 32) | ((psr >> 7) & 4);
  n_ = psr >> 31; z_ = (psr >> 30) & 1; c_ = (psr >> 29) & 1; v_ = (psr >> 28) & 1;
  q_ = (psr >> 27) & 1;
  itstate_ = ((psr >> 25) & 3) | (((psr >> 10) & 0x3f) << 2);
  ipsr_ = toThread ? 0 : (psr & 0x1ff);
  updateExecutionPriority();
  irqDirty_ = true;
  cycles_ += 10;
}

void CortexM3::halt(const char* reason) {
  halted_ = true;
  haltReason_ = reason;
}

void CortexM3::raiseFault(const char* reason) {
  if (haltReason_[0] == 0) { haltReason_ = reason; faultPc_ = pc_; }
  if (active_[3] || execPriority_ <= -1) { halt(reason); return; }
  hfsr_ |= 1u << 30;
  exceptionEntry(3, pc_);
}

void CortexM3::undefined(uint32_t) {
  cfsr_ |= 1u << 16;
  raiseFault("instruction non reconnue");
}

void CortexM3::reset() {
  std::memset(r_, 0, sizeof(r_));
  std::memset(pending_, 0, sizeof(pending_));
  std::memset(active_, 0, sizeof(active_));
  std::memset(enabled_, 0, sizeof(enabled_));
  std::memset(priority_, 0, sizeof(priority_));
  n_ = z_ = c_ = v_ = q_ = false;
  itstate_ = 0; ipsr_ = 0; otherSp_ = 0; usingPsp_ = false; controlNpriv_ = false;
  primask_ = faultmask_ = basepri_ = 0;
  execPriority_ = 256; irqDirty_ = false; sleeping_ = false;
  vtor_ = 0; aircrPrigroup_ = 0; ccr_ = 0; shcsr_ = 0; cfsr_ = hfsr_ = 0;
  systickCtrl_ = systickLoad_ = systickValueAtRef_ = 0;
  systickRefCycle_ = cycles_;
  systickNextFire_ = ~uint64_t { 0 };
  exclusive_ = false; halted_ = false; haltReason_ = ""; faultPc_ = 0;
  updateNextEvent();
  r_[13] = slowRead(0, 4) & ~3u;
  pc_ = slowRead(4, 4) & ~1u;
  nextPc_ = pc_;
  r_[14] = 0xffffffffu;
}

// --- Exécution --------------------------------------------------------------------------

void CortexM3::run(uint64_t untilCycle) {
  while (cycles_ < untilCycle && !halted_) {
    if (cycles_ >= nextEvent_) serviceDeadlines();
    if (irqDirty_) checkInterrupts();
    if (sleeping_) {
      // WFI : le processeur attend la prochaine interruption.
      cycles_ = std::max(cycles_, std::min(untilCycle, nextEvent_));
      continue;
    }
    // nextEvent_ peut changer pendant l'exécution (écriture dans un timer).
    while (cycles_ < untilCycle && cycles_ < nextEvent_ && !irqDirty_ && !sleeping_ && !halted_) step();
  }
}

bool CortexM3::conditionPassed(uint32_t cond) const {
  bool result;
  switch (cond >> 1) {
    case 0: result = z_; break;
    case 1: result = c_; break;
    case 2: result = n_; break;
    case 3: result = v_; break;
    case 4: result = c_ && !z_; break;
    case 5: result = n_ == v_; break;
    case 6: result = n_ == v_ && !z_; break;
    default: return true;
  }
  return (cond & 1) ? !result : result;
}

uint32_t CortexM3::addWithCarry(uint32_t x, uint32_t y, uint32_t carryIn, bool setFlags) {
  const uint64_t sum = static_cast<uint64_t>(x) + y + carryIn;
  const uint32_t result = static_cast<uint32_t>(sum);
  if (setFlags) {
    n_ = result >> 31;
    z_ = result == 0;
    c_ = (sum >> 32) & 1;
    v_ = ((x ^ result) & (y ^ result)) >> 31;
  }
  return result;
}

void CortexM3::branchTo(uint32_t address) {
  nextPc_ = address & ~1u;
  cycles_ += 2;
}

void CortexM3::bxWritePc(uint32_t address) {
  if (ipsr_ != 0 && (address & 0xf0000000u) == 0xf0000000u) {
    exceptionReturn(address);
    return;
  }
  if (!(address & 1)) { cfsr_ |= 1u << 17; raiseFault("saut vers du code non Thumb"); return; }
  branchTo(address);
}

void CortexM3::step() {
  uint32_t hw1;
  {
    const uint32_t o = pc_ - kFlashBase;
    if (o <= flashSize_ - 2) { uint16_t v; std::memcpy(&v, &flash_[o], 2); hw1 = v; }
    else hw1 = read16(pc_);
  }
  ++cycles_;
  ++instructions_;

  const uint32_t it = itstate_;
  if (it) {
    itstate_ = (it & 7) ? ((it & 0xe0) | ((it << 1) & 0x1f)) : 0;
  }

  if (hw1 < 0xe800) {
    nextPc_ = pc_ + 2;
    if (!it || conditionPassed(it >> 4)) {
      inIt_ = it != 0;
      exec16(hw1);
    }
  } else {
    const uint32_t insn = (hw1 << 16) | read16(pc_ + 2);
    nextPc_ = pc_ + 4;
    if (!it || conditionPassed(it >> 4)) exec32(insn);
  }
  pc_ = nextPc_;
}

void CortexM3::exec16(uint32_t insn) {
  const bool setFlags = !inIt_;
  switch (insn >> 11) {
    case 0: case 1: case 2: {  // LSL, LSR, ASR immédiat
      bool carry = c_;
      const uint32_t result = shiftImm(r_[(insn >> 3) & 7], insn >> 11, (insn >> 6) & 31, carry);
      r_[insn & 7] = result;
      if (setFlags) { setNZ(result); c_ = carry; }
      break;
    }
    case 3: {  // ADD / SUB registre ou immédiat 3 bits
      const uint32_t operand = (insn & 0x400) ? ((insn >> 6) & 7) : r_[(insn >> 6) & 7];
      const uint32_t rn = r_[(insn >> 3) & 7];
      r_[insn & 7] = (insn & 0x200) ? addWithCarry(rn, ~operand, 1, setFlags)
                                    : addWithCarry(rn, operand, 0, setFlags);
      break;
    }
    case 4:  // MOV immédiat
      r_[(insn >> 8) & 7] = insn & 0xff;
      if (setFlags) setNZ(insn & 0xff);
      break;
    case 5:  // CMP immédiat
      addWithCarry(r_[(insn >> 8) & 7], ~(insn & 0xff), 1, true);
      break;
    case 6:
      r_[(insn >> 8) & 7] = addWithCarry(r_[(insn >> 8) & 7], insn & 0xff, 0, setFlags);
      break;
    case 7:
      r_[(insn >> 8) & 7] = addWithCarry(r_[(insn >> 8) & 7], ~(insn & 0xff), 1, setFlags);
      break;
    case 8: {
      if (!(insn & 0x400)) {  // opérations sur registres bas
        const uint32_t rdn = insn & 7;
        const uint32_t rm = r_[(insn >> 3) & 7];
        const uint32_t x = r_[rdn];
        uint32_t result;
        switch ((insn >> 6) & 15) {
          case 0: result = x & rm; r_[rdn] = result; if (setFlags) setNZ(result); break;
          case 1: result = x ^ rm; r_[rdn] = result; if (setFlags) setNZ(result); break;
          case 2: case 3: case 4: case 7: {
            static const int types[8] = { 0, 0, kLSL, kLSR, kASR, 0, 0, kROR };
            bool carry = c_;
            result = shiftC(x, types[(insn >> 6) & 7], rm & 0xff, carry);
            r_[rdn] = result;
            if (setFlags) { setNZ(result); c_ = carry; }
            break;
          }
          case 5: r_[rdn] = addWithCarry(x, rm, c_, setFlags); break;
          case 6: r_[rdn] = addWithCarry(x, ~rm, c_, setFlags); break;
          case 8: setNZ(x & rm); break;
          case 9: r_[rdn] = addWithCarry(~rm, 0, 1, setFlags); break;
          case 10: addWithCarry(x, ~rm, 1, true); break;
          case 11: addWithCarry(x, rm, 0, true); break;
          case 12: result = x | rm; r_[rdn] = result; if (setFlags) setNZ(result); break;
          case 13: result = x * rm; r_[rdn] = result; if (setFlags) setNZ(result); break;
          case 14: result = x & ~rm; r_[rdn] = result; if (setFlags) setNZ(result); break;
          default: result = ~rm; r_[rdn] = result; if (setFlags) setNZ(result); break;
        }
      } else {  // registres hauts, BX, BLX
        const uint32_t rm = (insn >> 3) & 15;
        const uint32_t rdn = (insn & 7) | ((insn >> 4) & 8);
        switch ((insn >> 8) & 3) {
          case 0: {
            const uint32_t result = readReg(rdn) + readReg(rm);
            if (rdn == 15) aluWritePc(result); else r_[rdn] = result;
            break;
          }
          case 1: addWithCarry(readReg(rdn), ~readReg(rm), 1, true); break;
          case 2: {
            const uint32_t result = readReg(rm);
            if (rdn == 15) aluWritePc(result); else r_[rdn] = result;
            break;
          }
          default: {
            const uint32_t target = readReg(rm);
            if (insn & 0x80) r_[14] = (pc_ + 2) | 1;
            bxWritePc(target);
            break;
          }
        }
      }
      break;
    }
    case 9:  // LDR littéral
      r_[(insn >> 8) & 7] = read32(((pc_ + 4) & ~3u) + ((insn & 0xff) << 2));
      ++cycles_;
      break;
    case 10: case 11: {  // chargement / rangement avec décalage registre
      const uint32_t address = r_[(insn >> 3) & 7] + r_[(insn >> 6) & 7];
      const uint32_t rt = insn & 7;
      switch ((insn >> 9) & 7) {
        case 0: write32(address, r_[rt]); break;
        case 1: write16(address, r_[rt]); break;
        case 2: write8(address, r_[rt]); break;
        case 3: r_[rt] = static_cast<uint32_t>(static_cast<int8_t>(read8(address))); break;
        case 4: r_[rt] = read32(address); break;
        case 5: r_[rt] = read16(address); break;
        case 6: r_[rt] = read8(address); break;
        default: r_[rt] = static_cast<uint32_t>(static_cast<int16_t>(read16(address))); break;
      }
      ++cycles_;
      break;
    }
    case 12: write32(r_[(insn >> 3) & 7] + (((insn >> 6) & 31) << 2), r_[insn & 7]); ++cycles_; break;
    case 13: r_[insn & 7] = read32(r_[(insn >> 3) & 7] + (((insn >> 6) & 31) << 2)); ++cycles_; break;
    case 14: write8(r_[(insn >> 3) & 7] + ((insn >> 6) & 31), r_[insn & 7]); ++cycles_; break;
    case 15: r_[insn & 7] = read8(r_[(insn >> 3) & 7] + ((insn >> 6) & 31)); ++cycles_; break;
    case 16: write16(r_[(insn >> 3) & 7] + (((insn >> 6) & 31) << 1), r_[insn & 7]); ++cycles_; break;
    case 17: r_[insn & 7] = read16(r_[(insn >> 3) & 7] + (((insn >> 6) & 31) << 1)); ++cycles_; break;
    case 18: write32(r_[13] + ((insn & 0xff) << 2), r_[(insn >> 8) & 7]); ++cycles_; break;
    case 19: r_[(insn >> 8) & 7] = read32(r_[13] + ((insn & 0xff) << 2)); ++cycles_; break;
    case 20: r_[(insn >> 8) & 7] = ((pc_ + 4) & ~3u) + ((insn & 0xff) << 2); break;
    case 21: r_[(insn >> 8) & 7] = r_[13] + ((insn & 0xff) << 2); break;
    case 22: case 23: {
      switch ((insn >> 8) & 15) {
        case 0:
          if (insn & 0x80) r_[13] -= (insn & 0x7f) << 2; else r_[13] += (insn & 0x7f) << 2;
          break;
        case 1: case 3: case 9: case 11: {  // CBZ / CBNZ
          const bool nonZero = insn & 0x800;
          if ((r_[insn & 7] != 0) == nonZero) {
            branchTo(pc_ + 4 + (((insn >> 3) & 0x40) | ((insn >> 2) & 0x3e)));
          }
          break;
        }
        case 2: {
          const uint32_t rm = r_[(insn >> 3) & 7];
          switch ((insn >> 6) & 3) {
            case 0: r_[insn & 7] = static_cast<uint32_t>(static_cast<int16_t>(rm)); break;
            case 1: r_[insn & 7] = static_cast<uint32_t>(static_cast<int8_t>(rm)); break;
            case 2: r_[insn & 7] = rm & 0xffff; break;
            default: r_[insn & 7] = rm & 0xff; break;
          }
          break;
        }
        case 4: case 5: {  // PUSH
          const uint32_t list = (insn & 0xff) | ((insn & 0x100) << 6);
          const int count = popcount16(list);
          uint32_t address = r_[13] - 4 * static_cast<uint32_t>(count);
          r_[13] = address;
          for (int i = 0; i < 15; ++i) {
            if (list & (1u << i)) { write32(address, r_[i]); address += 4; }
          }
          cycles_ += static_cast<uint64_t>(count);
          break;
        }
        case 6:
          if ((insn & 0xffe8) == 0xb660) {  // CPSIE / CPSID
            const uint32_t disable = (insn >> 4) & 1;
            if (insn & 2) primask_ = disable;
            if (insn & 1) faultmask_ = disable;
            updateExecutionPriority();
            irqDirty_ = true;
          } else {
            undefined(insn);
          }
          break;
        case 10: {
          const uint32_t rm = r_[(insn >> 3) & 7];
          switch ((insn >> 6) & 3) {
            case 0: r_[insn & 7] = (rm >> 24) | ((rm >> 8) & 0xff00) | ((rm << 8) & 0xff0000) | (rm << 24); break;
            case 1: r_[insn & 7] = ((rm >> 8) & 0x00ff00ff) | ((rm << 8) & 0xff00ff00); break;
            case 3: r_[insn & 7] = static_cast<uint32_t>(static_cast<int16_t>(((rm >> 8) & 0xff) | ((rm << 8) & 0xff00))); break;
            default: undefined(insn); break;
          }
          break;
        }
        case 12: case 13: {  // POP
          const uint32_t list = insn & 0xff;
          uint32_t address = r_[13];
          for (int i = 0; i < 8; ++i) {
            if (list & (1u << i)) { r_[i] = read32(address); address += 4; }
          }
          int count = popcount16(list);
          if (insn & 0x100) {
            const uint32_t target = read32(address);
            address += 4;
            ++count;
            r_[13] = address;
            bxWritePc(target);
          } else {
            r_[13] = address;
          }
          cycles_ += static_cast<uint64_t>(count);
          break;
        }
        case 14: raiseFault("BKPT"); break;
        case 15:
          if (insn & 0xf) {
            itstate_ = insn & 0xff;  // IT
          } else if (((insn >> 4) & 15) == 3) {
            sleeping_ = true;        // WFI
            irqDirty_ = true;
          }
          break;
        default: undefined(insn); break;
      }
      break;
    }
    case 24: {  // STMIA
      const uint32_t rn = (insn >> 8) & 7;
      uint32_t address = r_[rn];
      int count = 0;
      for (int i = 0; i < 8; ++i) {
        if (insn & (1u << i)) { write32(address, r_[i]); address += 4; ++count; }
      }
      r_[rn] = address;
      cycles_ += static_cast<uint64_t>(count);
      break;
    }
    case 25: {  // LDMIA
      const uint32_t rn = (insn >> 8) & 7;
      uint32_t address = r_[rn];
      int count = 0;
      for (int i = 0; i < 8; ++i) {
        if (insn & (1u << i)) { r_[i] = read32(address); address += 4; ++count; }
      }
      if (!(insn & (1u << rn))) r_[rn] = address;
      cycles_ += static_cast<uint64_t>(count);
      break;
    }
    case 26: case 27: {
      const uint32_t cond = (insn >> 8) & 15;
      if (cond == 14) {
        undefined(insn);
      } else if (cond == 15) {
        pendException(11);  // SVC
      } else if (conditionPassed(cond)) {
        branchTo(pc_ + 4 + static_cast<uint32_t>(signExtend(insn & 0xff, 8) * 2));
      }
      break;
    }
    case 28:
      branchTo(pc_ + 4 + static_cast<uint32_t>(signExtend(insn & 0x7ff, 11) * 2));
      break;
    default: undefined(insn); break;
  }
}

void CortexM3::execDataProcessing(uint32_t op, bool setFlags, uint32_t rd, uint32_t rn,
                                  uint32_t operand2, bool shifterCarry) {
  const uint32_t x = r_[rn];
  uint32_t result;
  switch (op) {
    case 0:  // AND / TST
      result = x & operand2;
      if (rd == 15) { if (!setFlags) { undefined(0); return; } }
      else r_[rd] = result;
      if (setFlags) { setNZ(result); c_ = shifterCarry; }
      break;
    case 1:  // BIC
      result = x & ~operand2;
      r_[rd] = result;
      if (setFlags) { setNZ(result); c_ = shifterCarry; }
      break;
    case 2:  // ORR / MOV
      result = rn == 15 ? operand2 : (x | operand2);
      r_[rd] = result;
      if (setFlags) { setNZ(result); c_ = shifterCarry; }
      break;
    case 3:  // ORN / MVN
      result = rn == 15 ? ~operand2 : (x | ~operand2);
      r_[rd] = result;
      if (setFlags) { setNZ(result); c_ = shifterCarry; }
      break;
    case 4:  // EOR / TEQ
      result = x ^ operand2;
      if (rd == 15) { if (!setFlags) { undefined(0); return; } }
      else r_[rd] = result;
      if (setFlags) { setNZ(result); c_ = shifterCarry; }
      break;
    case 8:  // ADD / CMN
      result = addWithCarry(x, operand2, 0, setFlags);
      if (rd != 15) r_[rd] = result; else if (!setFlags) undefined(0);
      break;
    case 10: r_[rd] = addWithCarry(x, operand2, c_, setFlags); break;
    case 11: r_[rd] = addWithCarry(x, ~operand2, c_, setFlags); break;
    case 13:  // SUB / CMP
      result = addWithCarry(x, ~operand2, 1, setFlags);
      if (rd != 15) r_[rd] = result; else if (!setFlags) undefined(0);
      break;
    case 14: r_[rd] = addWithCarry(~x, operand2, 1, setFlags); break;
    default: undefined(0); break;
  }
}

void CortexM3::execLoadStoreMultiple(uint32_t insn) {
  const uint32_t hw1 = insn >> 16;
  const uint32_t list = insn & 0xffff;
  const uint32_t rn = hw1 & 15;
  const bool writeback = hw1 & 0x20;
  const bool load = hw1 & 0x10;
  const uint32_t mode = (hw1 >> 7) & 3;
  if (mode != 1 && mode != 2) { undefined(insn); return; }
  const int count = popcount16(list);
  const uint32_t size = 4 * static_cast<uint32_t>(count);
  uint32_t address = mode == 1 ? r_[rn] : r_[rn] - size;
  const uint32_t finalRn = mode == 1 ? r_[rn] + size : r_[rn] - size;
  if (load) {
    uint32_t target = 0;
    for (int i = 0; i < 15; ++i) {
      if (list & (1u << i)) { r_[i] = read32(address); address += 4; }
    }
    if (list & 0x8000) target = read32(address);
    if (writeback && !(list & (1u << rn))) r_[rn] = finalRn;
    if (list & 0x8000) bxWritePc(target);
  } else {
    for (int i = 0; i < 15; ++i) {
      if (list & (1u << i)) { write32(address, r_[i]); address += 4; }
    }
    if (writeback) r_[rn] = finalRn;
  }
  cycles_ += static_cast<uint64_t>(count);
}

void CortexM3::execDualExclusiveTable(uint32_t insn) {
  const uint32_t hw1 = insn >> 16;
  const uint32_t hw2 = insn & 0xffff;
  const uint32_t rn = hw1 & 15;
  const bool p = hw1 & 0x100, u = hw1 & 0x80, w = hw1 & 0x20, load = hw1 & 0x10;
  const uint32_t rt = hw2 >> 12;

  if (!p && !w) {
    if (!u) {
      const uint32_t address = r_[rn] + ((hw2 & 0xff) << 2);
      if (load) {  // LDREX
        r_[rt] = read32(address);
        exclusive_ = true;
      } else {     // STREX
        const uint32_t rd = (hw2 >> 8) & 15;
        if (exclusive_) { write32(address, r_[rt]); r_[rd] = 0; } else { r_[rd] = 1; }
        exclusive_ = false;
      }
      ++cycles_;
      return;
    }
    const uint32_t op3 = (hw2 >> 4) & 15;
    const uint32_t rm = hw2 & 15;
    if (load) {
      switch (op3) {
        case 0:  // TBB
          branchTo(pc_ + 4 + 2 * read8(readReg(rn) + r_[rm]));
          ++cycles_;
          return;
        case 1:  // TBH
          branchTo(pc_ + 4 + 2 * read16(readReg(rn) + 2 * r_[rm]));
          ++cycles_;
          return;
        case 4: r_[rt] = read8(r_[rn]); exclusive_ = true; ++cycles_; return;
        case 5: r_[rt] = read16(r_[rn]); exclusive_ = true; ++cycles_; return;
        default: undefined(insn); return;
      }
    }
    if (op3 == 4 || op3 == 5) {  // STREXB / STREXH
      if (exclusive_) {
        if (op3 == 4) write8(r_[rn], r_[rt]); else write16(r_[rn], r_[rt]);
        r_[rm] = 0;
      } else {
        r_[rm] = 1;
      }
      exclusive_ = false;
      ++cycles_;
      return;
    }
    undefined(insn);
    return;
  }

  // LDRD / STRD
  const uint32_t rt2 = (hw2 >> 8) & 15;
  const uint32_t imm = (hw2 & 0xff) << 2;
  const uint32_t base = rn == 15 ? ((pc_ + 4) & ~3u) : r_[rn];
  const uint32_t offset = u ? base + imm : base - imm;
  const uint32_t address = p ? offset : base;
  if (load) {
    const uint32_t a = read32(address);
    const uint32_t b = read32(address + 4);
    if (w) r_[rn] = offset;
    r_[rt] = a;
    r_[rt2] = b;
  } else {
    write32(address, r_[rt]);
    write32(address + 4, r_[rt2]);
    if (w) r_[rn] = offset;
  }
  cycles_ += 2;
}

void CortexM3::execLoadStoreSingle(uint32_t insn) {
  const uint32_t hw1 = insn >> 16;
  const uint32_t hw2 = insn & 0xffff;
  const uint32_t size = (hw1 >> 5) & 3;
  const bool load = hw1 & 0x10;
  const bool sign = hw1 & 0x100;
  const uint32_t rn = hw1 & 15;
  const uint32_t rt = hw2 >> 12;
  if (size == 3) { undefined(insn); return; }

  uint32_t address;
  bool writeback = false;
  uint32_t writebackValue = 0;
  if (rn == 15) {
    if (!load) { undefined(insn); return; }
    const uint32_t base = (pc_ + 4) & ~3u;
    address = (hw1 & 0x80) ? base + (hw2 & 0xfff) : base - (hw2 & 0xfff);
  } else if (hw1 & 0x80) {
    address = r_[rn] + (hw2 & 0xfff);
  } else if (hw2 & 0x800) {
    const uint32_t imm = hw2 & 0xff;
    const uint32_t offset = (hw2 & 0x200) ? r_[rn] + imm : r_[rn] - imm;
    address = (hw2 & 0x400) ? offset : r_[rn];
    writeback = hw2 & 0x100;
    writebackValue = offset;
    if (!(hw2 & 0x400) && !writeback) { undefined(insn); return; }
  } else if ((hw2 & 0xfc0) == 0) {
    address = r_[rn] + (r_[hw2 & 15] << ((hw2 >> 4) & 3));
  } else {
    undefined(insn);
    return;
  }

  ++cycles_;
  if (load) {
    if (rt == 15) {
      if (size != 2) return;  // PLD / PLI : sans effet
      const uint32_t target = read32(address);
      if (writeback) r_[rn] = writebackValue;
      bxWritePc(target);
      return;
    }
    uint32_t value;
    if (size == 0) {
      value = read8(address);
      if (sign) value = static_cast<uint32_t>(static_cast<int8_t>(value));
    } else if (size == 1) {
      value = read16(address);
      if (sign) value = static_cast<uint32_t>(static_cast<int16_t>(value));
    } else {
      value = read32(address);
    }
    if (writeback) r_[rn] = writebackValue;
    r_[rt] = value;
  } else {
    const uint32_t value = r_[rt];
    if (size == 0) write8(address, value);
    else if (size == 1) write16(address, value);
    else write32(address, value);
    if (writeback) r_[rn] = writebackValue;
  }
}

void CortexM3::execBranchMisc(uint32_t insn) {
  const uint32_t hw1 = insn >> 16;
  const uint32_t hw2 = insn & 0xffff;
  const uint32_t s = (hw1 >> 10) & 1;
  const uint32_t j1 = (hw2 >> 13) & 1;
  const uint32_t j2 = (hw2 >> 11) & 1;

  if (hw2 & 0x1000) {  // B.W inconditionnel ou BL
    const uint32_t i1 = !(j1 ^ s), i2 = !(j2 ^ s);
    const uint32_t imm = (s << 24) | (i1 << 23) | (i2 << 22) | ((hw1 & 0x3ff) << 12) |
                         ((hw2 & 0x7ff) << 1);
    const uint32_t target = pc_ + 4 + static_cast<uint32_t>(signExtend(imm, 25));
    if (hw2 & 0x4000) {
      r_[14] = (pc_ + 4) | 1;  // BL
    }
    branchTo(target);
    return;
  }
  if (hw2 & 0x4000) { undefined(insn); return; }  // BLX : pas sur Cortex-M

  const uint32_t cond = (hw1 >> 6) & 15;
  if (cond < 14) {  // B<cond>.W
    if (conditionPassed(cond)) {
      const uint32_t imm = (s << 20) | (j2 << 19) | (j1 << 18) | ((hw1 & 0x3f) << 12) |
                           ((hw2 & 0x7ff) << 1);
      branchTo(pc_ + 4 + static_cast<uint32_t>(signExtend(imm, 21)));
    }
    return;
  }

  const uint32_t op = (hw1 >> 4) & 0x7f;
  const uint32_t sysm = hw2 & 0xff;
  if (op == 0x38 || op == 0x39) {  // MSR
    const uint32_t value = r_[hw1 & 15];
    switch (sysm) {
      case 0: case 1: case 2: case 3:
        if (hw2 & 0x800) {
          n_ = value >> 31; z_ = (value >> 30) & 1; c_ = (value >> 29) & 1;
          v_ = (value >> 28) & 1; q_ = (value >> 27) & 1;
        }
        break;
      case 8: if (usingPsp_) otherSp_ = value & ~3u; else r_[13] = value & ~3u; break;
      case 9: if (usingPsp_) r_[13] = value & ~3u; else otherSp_ = value & ~3u; break;
      case 16: primask_ = value & 1; break;
      case 17: basepri_ = value & 0xf0; break;
      case 18:
        if ((value & 0xf0) != 0 && ((value & 0xf0) < basepri_ || basepri_ == 0)) basepri_ = value & 0xf0;
        break;
      case 19: faultmask_ = value & 1; break;
      case 20:
        controlNpriv_ = value & 1;
        if (ipsr_ == 0) switchStack(value & 2);
        break;
      default: break;
    }
    updateExecutionPriority();
    irqDirty_ = true;
    ++cycles_;
    return;
  }
  if (op == 0x3e || op == 0x3f) {  // MRS
    uint32_t value = 0;
    switch (sysm) {
      case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        if (sysm & 1) value |= ipsr_;
        if (!(sysm & 4)) value |= xpsr() & 0xf8000000u;
        break;
      case 8: value = usingPsp_ ? otherSp_ : r_[13]; break;
      case 9: value = usingPsp_ ? r_[13] : otherSp_; break;
      case 16: value = primask_; break;
      case 17: case 18: value = basepri_; break;
      case 19: value = faultmask_; break;
      case 20: value = (controlNpriv_ ? 1u : 0u) | (usingPsp_ ? 2u : 0u); break;
      default: break;
    }
    r_[(hw2 >> 8) & 15] = value;
    ++cycles_;
    return;
  }
  if (op == 0x3a) {  // NOP, WFI...
    if ((hw2 & 0x7ff) == 3) { sleeping_ = true; irqDirty_ = true; }
    return;
  }
  if (op == 0x3b) {  // CLREX, DSB, DMB, ISB
    if (((hw2 >> 4) & 15) == 2) exclusive_ = false;
    return;
  }
  undefined(insn);
}

void CortexM3::exec32(uint32_t insn) {
  const uint32_t hw1 = insn >> 16;
  const uint32_t hw2 = insn & 0xffff;

  if ((hw1 & 0xfe00) == 0xe800) {
    if (hw1 & 0x40) execDualExclusiveTable(insn); else execLoadStoreMultiple(insn);
    return;
  }

  if ((hw1 & 0xfe00) == 0xea00) {  // opération avec registre décalé
    bool carry = c_;
    const uint32_t imm5 = ((hw2 >> 10) & 0x1c) | ((hw2 >> 6) & 3);
    const uint32_t operand = shiftImm(r_[hw2 & 15], (hw2 >> 4) & 3, imm5, carry);
    execDataProcessing((hw1 >> 5) & 15, hw1 & 0x10, (hw2 >> 8) & 15, hw1 & 15, operand, carry);
    return;
  }

  if ((hw1 & 0xf800) == 0xf000) {
    if (hw2 & 0x8000) { execBranchMisc(insn); return; }
    const uint32_t rn = hw1 & 15;
    const uint32_t rd = (hw2 >> 8) & 15;
    const uint32_t imm12 = ((hw1 & 0x400) << 1) | ((hw2 >> 4) & 0x700) | (hw2 & 0xff);
    if (!(hw1 & 0x200)) {  // immédiat « modifié »
      bool carry = c_;
      const uint32_t operand = thumbExpandImm(imm12, carry);
      execDataProcessing((hw1 >> 5) & 15, hw1 & 0x10, rd, rn, operand, carry);
      return;
    }
    const uint32_t lsb = ((hw2 >> 10) & 0x1c) | ((hw2 >> 6) & 3);
    switch ((hw1 >> 4) & 0x1f) {
      case 0x00:  // ADDW / ADR
        r_[rd] = (rn == 15 ? ((pc_ + 4) & ~3u) : r_[rn]) + imm12;
        break;
      case 0x04:  // MOVW
        r_[rd] = ((hw1 & 15) << 12) | imm12;
        break;
      case 0x0a:  // SUBW / ADR
        r_[rd] = (rn == 15 ? ((pc_ + 4) & ~3u) : r_[rn]) - imm12;
        break;
      case 0x0c:  // MOVT
        r_[rd] = (r_[rd] & 0xffff) | ((((hw1 & 15) << 12) | imm12) << 16);
        break;
      case 0x10: case 0x12: {  // SSAT
        if ((hw1 & 0x20) && lsb == 0) { undefined(insn); return; }
        bool dummy = false;
        const int64_t operand = static_cast<int32_t>(
            (hw1 & 0x20) ? shiftC(r_[rn], kASR, lsb, dummy) : shiftC(r_[rn], kLSL, lsb, dummy));
        const int bits = static_cast<int>(hw2 & 0x1f) + 1;
        const int64_t hi = (int64_t { 1 } << (bits - 1)) - 1;
        const int64_t lo = -(int64_t { 1 } << (bits - 1));
        int64_t result = operand;
        if (result > hi) { result = hi; q_ = true; }
        else if (result < lo) { result = lo; q_ = true; }
        r_[rd] = static_cast<uint32_t>(result);
        break;
      }
      case 0x18: case 0x1a: {  // USAT
        if ((hw1 & 0x20) && lsb == 0) { undefined(insn); return; }
        bool dummy = false;
        const int64_t operand = static_cast<int32_t>(
            (hw1 & 0x20) ? shiftC(r_[rn], kASR, lsb, dummy) : shiftC(r_[rn], kLSL, lsb, dummy));
        const int bits = static_cast<int>(hw2 & 0x1f);
        const int64_t hi = (int64_t { 1 } << bits) - 1;
        int64_t result = operand;
        if (result > hi) { result = hi; q_ = true; }
        else if (result < 0) { result = 0; q_ = true; }
        r_[rd] = static_cast<uint32_t>(result);
        break;
      }
      case 0x14: {  // SBFX
        const uint32_t width = (hw2 & 0x1f) + 1;
        if (lsb + width > 32) { undefined(insn); return; }
        r_[rd] = static_cast<uint32_t>(
            static_cast<int32_t>(r_[rn] << (32 - lsb - width)) >> (32 - width));
        break;
      }
      case 0x1c: {  // UBFX
        const uint32_t width = (hw2 & 0x1f) + 1;
        if (lsb + width > 32) { undefined(insn); return; }
        r_[rd] = (r_[rn] << (32 - lsb - width)) >> (32 - width);
        break;
      }
      case 0x16: {  // BFI / BFC
        const uint32_t msb = hw2 & 0x1f;
        if (msb < lsb) { undefined(insn); return; }
        const uint32_t width = msb - lsb + 1;
        const uint32_t mask = (width == 32 ? 0xffffffffu : ((1u << width) - 1)) << lsb;
        const uint32_t source = rn == 15 ? 0 : (r_[rn] << lsb);
        r_[rd] = (r_[rd] & ~mask) | (source & mask);
        break;
      }
      default: undefined(insn); break;
    }
    return;
  }

  if ((hw1 & 0xfe00) == 0xf800) { execLoadStoreSingle(insn); return; }

  const uint32_t rn = hw1 & 15;
  const uint32_t rd = (hw2 >> 8) & 15;
  const uint32_t rm = hw2 & 15;

  if ((hw1 & 0xff00) == 0xfa00) {
    if (!(hw1 & 0x80)) {
      if ((hw2 & 0xf0f0) == 0xf000) {  // LSL / LSR / ASR / ROR par registre
        bool carry = c_;
        const uint32_t result = shiftC(r_[rn], (hw1 >> 5) & 3, r_[rm] & 0xff, carry);
        r_[rd] = result;
        if (hw1 & 0x10) { setNZ(result); c_ = carry; }
        return;
      }
      if ((hw2 & 0xf080) == 0xf080) {  // extensions de signe / zéro
        const uint32_t rotated = ror32(r_[rm], ((hw2 >> 4) & 3) * 8);
        uint32_t value;
        switch ((hw1 >> 4) & 7) {
          case 0: value = static_cast<uint32_t>(static_cast<int16_t>(rotated)); break;
          case 1: value = rotated & 0xffff; break;
          case 4: value = static_cast<uint32_t>(static_cast<int8_t>(rotated)); break;
          case 5: value = rotated & 0xff; break;
          default: undefined(insn); return;
        }
        r_[rd] = rn == 15 ? value : r_[rn] + value;
        return;
      }
      undefined(insn);
      return;
    }
    if ((hw2 & 0xf0c0) == 0xf080) {
      const uint32_t x = r_[rm];
      const uint32_t op = ((hw1 >> 4) & 3) << 2 | ((hw2 >> 4) & 3);
      switch (op) {
        case 4: r_[rd] = (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24); return;
        case 5: r_[rd] = ((x >> 8) & 0x00ff00ff) | ((x << 8) & 0xff00ff00); return;
        case 6: {
          uint32_t y = 0;
          for (int i = 0; i < 32; ++i) if (x & (1u << i)) y |= 0x80000000u >> i;
          r_[rd] = y;
          return;
        }
        case 7: r_[rd] = static_cast<uint32_t>(static_cast<int16_t>(((x >> 8) & 0xff) | ((x << 8) & 0xff00))); return;
        case 12: r_[rd] = static_cast<uint32_t>(clz32(x)); return;
        default: break;
      }
    }
    undefined(insn);
    return;
  }

  if ((hw1 & 0xff80) == 0xfb00) {  // multiplications 32 bits
    const uint32_t ra = hw2 >> 12;
    const uint32_t op = ((hw1 >> 4) & 7) << 2 | ((hw2 >> 4) & 3);
    if ((hw2 & 0xc0) != 0) { undefined(insn); return; }
    if (op == 0) {
      if (ra == 15) { r_[rd] = r_[rn] * r_[rm]; }
      else { r_[rd] = r_[ra] + r_[rn] * r_[rm]; ++cycles_; }
      return;
    }
    if (op == 1) { r_[rd] = r_[ra] - r_[rn] * r_[rm]; ++cycles_; return; }
    undefined(insn);
    return;
  }

  if ((hw1 & 0xff80) == 0xfb80) {  // multiplications 64 bits, divisions
    const uint32_t rdLo = hw2 >> 12;
    const uint32_t op2 = (hw2 >> 4) & 15;
    switch ((hw1 >> 4) & 7) {
      case 0: {  // SMULL
        if (op2 != 0) break;
        const int64_t result = static_cast<int64_t>(static_cast<int32_t>(r_[rn])) * static_cast<int32_t>(r_[rm]);
        r_[rdLo] = static_cast<uint32_t>(result);
        r_[rd] = static_cast<uint32_t>(static_cast<uint64_t>(result) >> 32);
        cycles_ += 3;
        return;
      }
      case 2: {  // UMULL
        if (op2 != 0) break;
        const uint64_t result = static_cast<uint64_t>(r_[rn]) * r_[rm];
        r_[rdLo] = static_cast<uint32_t>(result);
        r_[rd] = static_cast<uint32_t>(result >> 32);
        cycles_ += 3;
        return;
      }
      case 4: {  // SMLAL
        if (op2 != 0) break;
        const int64_t acc = static_cast<int64_t>((static_cast<uint64_t>(r_[rd]) << 32) | r_[rdLo]);
        const uint64_t result = static_cast<uint64_t>(acc) + static_cast<uint64_t>(
            static_cast<int64_t>(static_cast<int32_t>(r_[rn])) * static_cast<int32_t>(r_[rm]));
        r_[rdLo] = static_cast<uint32_t>(result);
        r_[rd] = static_cast<uint32_t>(result >> 32);
        cycles_ += 4;
        return;
      }
      case 6: {  // UMLAL
        if (op2 != 0) break;
        const uint64_t result = ((static_cast<uint64_t>(r_[rd]) << 32) | r_[rdLo]) +
                                static_cast<uint64_t>(r_[rn]) * r_[rm];
        r_[rdLo] = static_cast<uint32_t>(result);
        r_[rd] = static_cast<uint32_t>(result >> 32);
        cycles_ += 4;
        return;
      }
      case 1: {  // SDIV
        if (op2 != 15) break;
        const int32_t a = static_cast<int32_t>(r_[rn]);
        const int32_t b = static_cast<int32_t>(r_[rm]);
        if (b == 0) r_[rd] = 0;
        else if (a == INT32_MIN && b == -1) r_[rd] = 0x80000000u;
        else r_[rd] = static_cast<uint32_t>(a / b);
        const uint32_t ma = a < 0 ? 0u - static_cast<uint32_t>(a) : static_cast<uint32_t>(a);
        const uint32_t mb = b < 0 ? 0u - static_cast<uint32_t>(b) : static_cast<uint32_t>(b);
        cycles_ += static_cast<uint64_t>(std::clamp(1 + (clz32(mb) - clz32(ma)) / 4, 1, 11));
        return;
      }
      case 3: {  // UDIV
        if (op2 != 15) break;
        const uint32_t a = r_[rn], b = r_[rm];
        r_[rd] = b == 0 ? 0 : a / b;
        cycles_ += static_cast<uint64_t>(std::clamp(1 + (clz32(b) - clz32(a)) / 4, 1, 11));
        return;
      }
      default: break;
    }
    undefined(insn);
    return;
  }

  undefined(insn);
}

}  // namespace emu
}  // namespace tresses
