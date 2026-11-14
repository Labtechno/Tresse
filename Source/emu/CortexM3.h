// Tresses — émulateur de processeur ARM Cortex-M3 (ARMv7-M, Thumb / Thumb-2).
//
// C'est le processeur du STM32F103 de Braids. L'émulateur exécute le fichier du
// firmware tel quel, instruction par instruction, et compte les cycles d'horloge
// consommés (valeurs de la documentation ARM, voir cycles dans CortexM3.cpp).
//
// Écrit pour Tresses, sans code tiers. Licence MIT.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace tresses {
namespace emu {

// Tout ce qui n'est ni la flash ni la RAM : périphériques du microcontrôleur.
class Bus {
 public:
  virtual ~Bus() = default;
  // size = 1, 2 ou 4 octets.
  virtual uint32_t read(uint32_t address, int size) = 0;
  virtual void write(uint32_t address, uint32_t value, int size) = 0;
  // Appelé quand le compteur de cycles atteint l'échéance fixée par
  // setExternalDeadline() : c'est là que les timers lèvent leurs interruptions.
  virtual void onDeadline(uint64_t cycles) = 0;
};

class CortexM3 {
 public:
  static constexpr uint32_t kFlashBase = 0x08000000;
  static constexpr uint32_t kRamBase = 0x20000000;
  static constexpr int kNumIrqs = 68;           // STM32F103
  static constexpr int kNumExceptions = 16 + kNumIrqs;

  CortexM3(size_t flashSize, size_t ramSize, Bus* bus);

  uint8_t* flash() { return flash_.data(); }
  size_t flashSize() const { return flashSize_; }
  uint8_t* ram() { return ram_.data(); }
  size_t ramSize() const { return ramSize_; }

  // Remise à zéro : charge SP et PC depuis la table des vecteurs (début de flash).
  void reset();

  // Exécute jusqu'à ce que le compteur de cycles atteigne `untilCycle`.
  void run(uint64_t untilCycle);

  uint64_t cycles() const { return cycles_; }
  uint64_t instructions() const { return instructions_; }

  // Cycles passés dans une boucle d'attente (le programme tourne en rond sans rien
  // modifier, en attendant une interruption). L'émulateur les saute d'un bloc :
  // le résultat est strictement le même, mais le calcul est bien plus rapide.
  // Le rapport idleCycles / cycles donne aussi le temps libre du processeur.
  uint64_t idleCycles() const { return idleCycles_; }

  // Demande à run() de rendre la main après l'instruction en cours.
  void stop() { stopRequested_ = true; irqDirty_ = true; }

  // Le processeur est bloqué pendant `cycles` (écriture en flash, par exemple).
  void stall(uint64_t cycles) { cycles_ += cycles; }

  // Interruptions externes (numéro d'IRQ du STM32, 0..67).
  void setIrqPending(int irq, bool pending);

  // Prochaine échéance des périphériques (en cycles). ~0 = aucune.
  void setExternalDeadline(uint64_t cycle);

  // Arrêt sur erreur fatale (instruction inconnue en HardFault, lockup...).
  bool halted() const { return halted_; }
  const char* haltReason() const { return haltReason_; }
  uint32_t faultPc() const { return faultPc_; }
  void halt(const char* reason);

  // Registres, pour les tests et le débogage.
  uint32_t reg(int n) const { return r_[n]; }
  void setReg(int n, uint32_t v) { r_[n] = v; }
  uint32_t pc() const { return pc_; }
  uint32_t xpsr() const;

  // Accès mémoire (utilisé aussi par les périphériques, ex. DMA).
  uint32_t read8(uint32_t a);
  uint32_t read16(uint32_t a);
  uint32_t read32(uint32_t a);
  void write8(uint32_t a, uint32_t v);
  void write16(uint32_t a, uint32_t v);
  void write32(uint32_t a, uint32_t v);

 private:
  // --- Exécution -----------------------------------------------------------------
  void step();
  void exec16(uint32_t insn);
  void exec32(uint32_t insn);
  void execDataProcessing(uint32_t op, bool setFlags, uint32_t rd, uint32_t rn,
                          uint32_t operand2, bool shifterCarry);
  void execLoadStoreSingle(uint32_t insn);
  void execLoadStoreMultiple(uint32_t insn);
  void execDualExclusiveTable(uint32_t insn);
  void execBranchMisc(uint32_t insn);
  void undefined(uint32_t insn);

  uint32_t slowRead(uint32_t a, int size);
  void slowWrite(uint32_t a, uint32_t v, int size);
  uint32_t scsRead(uint32_t a);
  void scsWrite(uint32_t a, uint32_t v);

  uint32_t readReg(uint32_t n) const { return n == 15 ? pc_ + 4 : r_[n]; }
  void branchTo(uint32_t address);       // BranchWritePC
  void branchDirect(uint32_t address);   // B, B<cond> : détecte les boucles d'attente
  void bxWritePc(uint32_t address);      // BXWritePC / LoadWritePC
  void aluWritePc(uint32_t address) { branchTo(address); }

  bool conditionPassed(uint32_t cond) const;
  void setNZ(uint32_t result) { n_ = result >> 31; z_ = result == 0; }
  uint32_t addWithCarry(uint32_t x, uint32_t y, uint32_t carryIn, bool setFlags);

  // --- Exceptions ----------------------------------------------------------------
  void pendException(int number) { pending_[number] = true; irqDirty_ = true; }
  void checkInterrupts();
  void exceptionEntry(int number, uint32_t returnAddress);
  void exceptionReturn(uint32_t excReturn);
  int groupPriority(int number) const;
  void updateExecutionPriority();
  void raiseFault(const char* reason);
  uint32_t& currentSp() { return r_[13]; }
  void switchStack(bool usePsp);

  // --- SysTick ---------------------------------------------------------------------
  uint32_t systickCurrent() const;
  void systickReschedule();
  void serviceDeadlines();
  void updateNextEvent();

  // --- État ----------------------------------------------------------------------
  uint32_t r_[16] = {};
  uint32_t pc_ = 0;        // adresse de l'instruction en cours
  uint32_t nextPc_ = 0;
  bool n_ = false, z_ = false, c_ = false, v_ = false, q_ = false;
  uint32_t itstate_ = 0;
  bool inIt_ = false;      // instruction 16 bits en cours dans un bloc IT
  uint32_t ipsr_ = 0;
  uint32_t otherSp_ = 0;   // la pile non sélectionnée (PSP ou MSP)
  bool usingPsp_ = false;
  bool controlNpriv_ = false;
  uint32_t primask_ = 0, faultmask_ = 0, basepri_ = 0;

  bool pending_[kNumExceptions] = {};
  bool active_[kNumExceptions] = {};
  bool enabled_[kNumExceptions] = {};
  uint8_t priority_[kNumExceptions] = {};
  int execPriority_ = 256;
  bool irqDirty_ = false;
  bool sleeping_ = false;

  uint32_t vtor_ = 0;
  uint32_t aircrPrigroup_ = 0;
  uint32_t ccr_ = 0;
  uint32_t shcsr_ = 0;
  uint32_t cfsr_ = 0, hfsr_ = 0;

  uint32_t systickCtrl_ = 0, systickLoad_ = 0;
  uint32_t systickValueAtRef_ = 0;
  uint64_t systickRefCycle_ = 0;
  uint64_t systickNextFire_ = ~uint64_t { 0 };

  uint64_t cycles_ = 0;
  uint64_t instructions_ = 0;
  uint64_t externalDeadline_ = ~uint64_t { 0 };
  uint64_t nextEvent_ = ~uint64_t { 0 };

  bool exclusive_ = false;

  // Détection des boucles d'attente (voir idleCycles()).
  struct LoopSnapshot {
    uint32_t pc = 0xffffffffu;
    uint32_t r[15] = {};
    uint32_t status = 0;
  } snapshot_;
  bool stateChanged_ = true;     // une écriture a modifié la mémoire ou un périphérique
  uint64_t snapshotCycles_ = 0, snapshotInstructions_ = 0;
  uint64_t idleCycles_ = 0;
  uint64_t runLimit_ = 0;
  bool stopRequested_ = false;
  bool halted_ = false;
  const char* haltReason_ = "";
  uint32_t faultPc_ = 0;

  std::vector<uint8_t> flash_, ram_;
  size_t flashSize_, ramSize_;
  Bus* bus_;
};

}  // namespace emu
}  // namespace tresses
