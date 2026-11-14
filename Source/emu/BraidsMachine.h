// Tresses — le module Braids émulé : un STM32F103 (CortexM3.h) entouré des
// composants de la carte, tels que le firmware les voit à travers les registres.
//
//   TIM1 ........ horloge audio (interruption à 96 kHz avec le firmware officiel)
//   SPI1 + PB0 .. ADC externe MCP3204 : TIMBRE, COLOR, V/OCT, FM
//   SPI2 + PB12 . DAC audio 16 bits
//   ADC1 + DMA .. potard FINE (ADC interne)
//   PA4 ......... entrée TRIG        PC13/14/15 ... encodeur (bouton, A, B)
//   PB1/10/11 ... registre à décalage de l'afficheur, PB5..8 : choix du caractère
//   FLASH ....... programmation et effacement (réglages du firmware)
//
// Le firmware est une image binaire chargée telle quelle. Licence MIT.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "CortexM3.h"

namespace tresses {
namespace emu {

class BraidsMachine : public Bus {
 public:
  static constexpr size_t kFlashSize = 128 * 1024;   // STM32F103CB
  static constexpr size_t kRamSize = 20 * 1024;
  static constexpr uint32_t kApplicationBase = 0x08004000;  // après le bootloader
  static constexpr uint32_t kCpuHz = 72000000;

  // Entrées du module pendant un échantillon audio.
  struct Inputs {
    uint16_t adc[4] = { 2048, 2048, 1638, 2048 };  // codes 12 bits de l'ADC externe
    uint16_t fine = 32768;                          // registre 16 bits de l'ADC interne
    bool trig = false;
  };

  BraidsMachine();

  // Charge une image d'application (.bin, prévue pour l'adresse 0x08004000).
  // Renvoie false si l'image n'est pas plausible.
  bool loadApplication(const uint8_t* data, size_t size);

  // Flash complète (firmware + réglages), pour l'enregistrer dans le projet.
  const uint8_t* flashImage() { return cpu_.flash(); }
  void restoreFlash(const uint8_t* data, size_t size);
  uint32_t flashWriteCount() const { return flashWrites_; }
  // Échantillons écoulés entre la mise sous tension et la première interruption audio.
  uint32_t bootSamples() const { return bootSamples_; }

  void setUniqueId(const uint32_t uid[3]) { for (int i = 0; i < 3; ++i) uid_[i] = uid[i]; }

  // Mise sous tension : remet le processeur et les périphériques à zéro.
  void powerOn();

  // Fait tourner le module jusqu'à obtenir n échantillons de sortie (un par
  // période du timer audio). `in[i]` est appliqué pendant l'échantillon i.
  void run(const Inputs* in, float* out, int n);

  // Fréquence du timer audio programmée par le firmware (0 tant qu'il n'a pas démarré).
  double sampleRate() const;

  // Encodeur : crans à jouer (+ = horaire) et bouton.
  void encoderTurn(int steps) { encoderPending_ += steps; }
  void encoderPress(bool pressed) { encoderPressed_ = pressed; }

  // Afficheur : mot de 14 segments de chaque caractère (mêmes bits que la police
  // du firmware) et proportion du temps pendant laquelle il est allumé (0..1).
  struct Display {
    uint16_t segments[4];
    float brightness;
  };
  Display display() const;

  uint64_t cycles() const { return cpu_.cycles(); }
  bool crashed() const { return cpu_.halted(); }
  const char* crashReason() const { return cpu_.haltReason(); }
  CortexM3& cpu() { return cpu_; }

  // Pour les essais : impose un nombre de cycles d'attente de la flash, quel que
  // soit le réglage du firmware (-1 = suivre le firmware). Sert à simuler un
  // processeur trop lent et à observer le comportement en surcharge.
  void setFlashWaitOverride(int waitStates) { flashWaitOverride_ = waitStates; }

  // --- Bus ---------------------------------------------------------------------
  uint32_t read(uint32_t address, int size) override;
  void write(uint32_t address, uint32_t value, int size) override;
  void onDeadline(uint64_t cycles) override;

 private:
  uint32_t readRegister(uint32_t address);
  void writeRegister(uint32_t address, uint32_t value);
  void gpioWrite(int port, uint32_t setMask, uint32_t clearMask);
  uint32_t gpioInput(int port) const;
  void timerReschedule();
  void timerUpdateEvent(uint64_t at);
  void sampleTick(uint64_t at);
  void advanceEncoder();
  void flashProgram(uint32_t address, uint32_t value, int size);

  CortexM3 cpu_;
  uint32_t uid_[3] = { 0x54726573, 0x73657321, 0x00000001 };
  std::unordered_map<uint32_t, uint32_t> otherRegisters_;

  // RCC
  uint32_t rccCr_ = 0x83, rccCfgr_ = 0;

  // GPIO A, B, C
  uint32_t gpioOdr_[3] = {};
  uint32_t gpioCrl_[3] = {}, gpioCrh_[3] = {};

  // TIM1
  uint32_t tim1Cr1_ = 0, tim1Dier_ = 0, tim1Sr_ = 0, tim1Psc_ = 0, tim1Arr_ = 0xffff;
  uint64_t tim1NextUpdate_ = ~uint64_t { 0 };
  uint64_t fallbackNext_ = ~uint64_t { 0 };

  // SPI1 + MCP3204
  uint32_t spi1Dr_ = 0;
  int adcByteIndex_ = 0;
  uint32_t adcFirstByte_ = 0;
  uint16_t adcSample_ = 0;

  // SPI2 + DAC
  int dacWordIndex_ = 0;
  uint32_t dacHigh_ = 0;
  uint16_t dacCode_ = 32768;

  // ADC1 + DMA1 canal 1
  uint32_t adc1Cr2_ = 0;
  uint32_t dmaCcr_ = 0, dmaCmar_ = 0;

  // FLASH
  uint32_t flashCr_ = 0x80, flashAr_ = 0;
  uint32_t flashWrites_ = 0;
  uint32_t bootSamples_ = 0;
  int flashWaitOverride_ = -1;

  // Afficheur
  uint16_t shiftRegister_ = 0;
  uint16_t latched_ = 0;
  uint16_t segments_[4] = {};
  uint64_t litSince_ = 0;
  uint64_t litCycles_ = 0, litWindowStart_ = 0;
  float brightness_ = 0.0f;

  // Encodeur
  int encoderPending_ = 0;
  bool encoderPressed_ = false;
  int encoderPhase_ = 0, encoderDirection_ = 0;
  bool encoderA_ = true, encoderB_ = true;
  uint64_t encoderNextPhase_ = 0;

  // Entrées / sorties de run()
  Inputs inputs_;
  const Inputs* runInputs_ = nullptr;
  float* runOut_ = nullptr;
  int runCount_ = 0, runIndex_ = 0;
};

}  // namespace emu
}  // namespace tresses
