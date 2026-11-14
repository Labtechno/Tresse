// Tresses — le module Braids émulé (voir BraidsMachine.h). Licence MIT.
//
// Références : STM32F103 Reference Manual (RM0008) pour les registres, fiches
// techniques du MCP3204 (ADC) et du DAC 16 bits pour les trames SPI, et les
// drivers du firmware (braids/drivers) pour le câblage des broches.

#include "BraidsMachine.h"

#include <algorithm>
#include <cstring>

namespace tresses {
namespace emu {

namespace {

constexpr uint32_t kGpioA = 0x40010800, kGpioB = 0x40010c00, kGpioC = 0x40011000;
constexpr uint32_t kTim1 = 0x40012c00, kSpi1 = 0x40013000, kSpi2 = 0x40003800;
constexpr uint32_t kAdc1 = 0x40012400, kDma1 = 0x40020000;
constexpr uint32_t kRcc = 0x40021000, kFlashReg = 0x40022000;

constexpr int kTim1UpIrq = 25;
constexpr uint64_t kFallbackSamplePeriod = 750;        // 96 kHz tant que TIM1 est arrêté
constexpr uint64_t kEncoderPhaseCycles = 144000;       // 2 ms par phase de quadrature
constexpr uint64_t kFlashProgramCycles = 3744;         // ~52 µs par demi-mot (fiche technique)
constexpr uint64_t kFlashEraseCycles = 1440000;        // ~20 ms par page
constexpr uint64_t kBrightnessWindow = 1440000;        // 20 ms
constexpr uint64_t kNever = ~uint64_t { 0 };

}  // namespace

BraidsMachine::BraidsMachine() : cpu_(kFlashSize, kRamSize, this) {}

bool BraidsMachine::loadApplication(const uint8_t* data, size_t size) {
  const uint32_t offset = kApplicationBase - CortexM3::kFlashBase;
  if (data == nullptr || size < 8 || size > kFlashSize - offset) return false;
  uint32_t sp, pc;
  std::memcpy(&sp, data, 4);
  std::memcpy(&pc, data + 4, 4);
  if (sp < CortexM3::kRamBase || sp > CortexM3::kRamBase + kRamSize) return false;
  if (pc < kApplicationBase || pc >= kApplicationBase + size || !(pc & 1)) return false;

  uint8_t* flash = cpu_.flash();
  // Comme le bootloader : la zone de l'application est effacée puis écrite ; les
  // 16 derniers Ko (réglages) ne sont touchés que si l'image va jusque-là.
  const size_t settingsStart = kFlashSize - 16 * 1024;
  std::memset(flash, 0xff, std::max(settingsStart, offset + size));
  std::memcpy(flash + offset, data, size);
  // À la place du bootloader : la remise à zéro saute directement à l'application.
  std::memcpy(flash, data, 8);
  return true;
}

void BraidsMachine::restoreFlash(const uint8_t* data, size_t size) {
  std::memcpy(cpu_.flash(), data, std::min(size, kFlashSize));
}

void BraidsMachine::powerOn() {
  std::memset(cpu_.ram(), 0, kRamSize);
  otherRegisters_.clear();
  rccCr_ = 0x83; rccCfgr_ = 0;
  std::memset(gpioOdr_, 0, sizeof(gpioOdr_));
  tim1Cr1_ = tim1Dier_ = tim1Sr_ = tim1Psc_ = 0; tim1Arr_ = 0xffff;
  tim1NextUpdate_ = kNever;
  spi1Dr_ = 0; adcByteIndex_ = 0; dacWordIndex_ = 0; dacCode_ = 32768;
  adc1Cr2_ = 0; dmaCcr_ = 0; dmaCmar_ = 0;
  flashCr_ = 0x80; flashAr_ = 0;
  shiftRegister_ = latched_ = 0;
  std::memset(segments_, 0, sizeof(segments_));
  litCycles_ = 0; brightness_ = 0.0f;
  encoderPending_ = 0; encoderPhase_ = 0; encoderA_ = encoderB_ = true;
  bootSamples_ = 0;
  cpu_.setFlashTiming(0, true);   // à la mise sous tension : pas de cycle d'attente
  cpu_.reset();
  litWindowStart_ = litSince_ = cpu_.cycles();
  encoderNextPhase_ = cpu_.cycles();
  fallbackNext_ = cpu_.cycles() + kFallbackSamplePeriod;
  cpu_.setExternalDeadline(fallbackNext_);
}

double BraidsMachine::sampleRate() const {
  if (!(tim1Cr1_ & 1)) return 96000.0;
  return static_cast<double>(kCpuHz) /
         (static_cast<double>(tim1Psc_ + 1) * static_cast<double>(tim1Arr_ + 1));
}

BraidsMachine::Display BraidsMachine::display() const {
  Display d;
  for (int i = 0; i < 4; ++i) d.segments[i] = segments_[i];
  d.brightness = brightness_;
  return d;
}

// --- Exécution ---------------------------------------------------------------------

void BraidsMachine::run(const Inputs* in, float* out, int n) {
  if (n <= 0) return;
  runInputs_ = in; runOut_ = out; runCount_ = n; runIndex_ = 0;
  inputs_ = in[0];
  while (runIndex_ < runCount_ && !cpu_.halted()) {
    cpu_.run(cpu_.cycles() + 4 * kFallbackSamplePeriod * static_cast<uint64_t>(runCount_ - runIndex_));
  }
  const float last = (32768.0f - static_cast<float>(dacCode_)) * (1.0f / 32768.0f);
  for (; runIndex_ < runCount_; ++runIndex_) out[runIndex_] = last;   // processeur arrêté
  runInputs_ = nullptr; runOut_ = nullptr;
}

void BraidsMachine::onDeadline(uint64_t now) {
  const bool timerRunning = tim1Cr1_ & 1;
  if (timerRunning) {
    while (now >= tim1NextUpdate_) {
      timerUpdateEvent(tim1NextUpdate_);
      tim1NextUpdate_ += static_cast<uint64_t>(tim1Psc_ + 1) * (tim1Arr_ + 1);
    }
    fallbackNext_ = kNever;
  } else {
    while (now >= fallbackNext_) {
      sampleTick(fallbackNext_);
      ++bootSamples_;
      fallbackNext_ += kFallbackSamplePeriod;
    }
  }
  cpu_.setExternalDeadline(timerRunning ? tim1NextUpdate_ : fallbackNext_);
}

void BraidsMachine::timerUpdateEvent(uint64_t at) {
  sampleTick(at);
  if (!(tim1Dier_ & 1)) ++bootSamples_;
  tim1Sr_ |= 1;
  if (tim1Dier_ & 1) cpu_.setIrqPending(kTim1UpIrq, true);
}

// Une période d'échantillonnage vient de s'écouler : la sortie du DAC est relevée
// et les entrées de l'échantillon suivant sont appliquées.
void BraidsMachine::sampleTick(uint64_t at) {
  if (runOut_ != nullptr && runIndex_ < runCount_) {
    // Le firmware envoie 32768 - échantillon ; l'étage de sortie remet le signe.
    runOut_[runIndex_] = (32768.0f - static_cast<float>(dacCode_)) * (1.0f / 32768.0f);
    ++runIndex_;
    if (runIndex_ < runCount_) inputs_ = runInputs_[runIndex_];
    else cpu_.stop();
  }
  // ADC interne en conversion continue, recopié en mémoire par le DMA (potard FINE).
  if ((dmaCcr_ & 1) && (adc1Cr2_ & 0x101) == 0x101 && dmaCmar_ >= CortexM3::kRamBase &&
      dmaCmar_ < CortexM3::kRamBase + kRamSize - 1) {
    cpu_.write16(dmaCmar_, inputs_.fine);
  }
  if (at >= encoderNextPhase_) advanceEncoder();
  if (at - litWindowStart_ >= kBrightnessWindow) {
    uint64_t lit = litCycles_;
    if (gpioOdr_[1] & 0x1e0) { lit += at - litSince_; litSince_ = at; }
    brightness_ = std::min(1.0f, static_cast<float>(lit) / static_cast<float>(at - litWindowStart_));
    litCycles_ = 0;
    litWindowStart_ = at;
  }
}

// Un cran horaire : A descend puis B ; (A,B) = 01, 00, 10, 11. L'inverse pour
// un cran anti-horaire. Chaque phase dure 2 ms, comme un encodeur tourné vite.
void BraidsMachine::advanceEncoder() {
  encoderNextPhase_ = cpu_.cycles() + kEncoderPhaseCycles;
  if (encoderPhase_ == 0) {
    if (encoderPending_ == 0) return;
    encoderDirection_ = encoderPending_ > 0 ? 1 : -1;
    encoderPending_ -= encoderDirection_;
  }
  const bool cw = encoderDirection_ > 0;
  switch (encoderPhase_) {
    case 0: encoderA_ = !cw; encoderB_ = cw; break;
    case 1: encoderA_ = false; encoderB_ = false; break;
    case 2: encoderA_ = cw; encoderB_ = !cw; break;
    default: encoderA_ = true; encoderB_ = true; break;
  }
  encoderPhase_ = (encoderPhase_ + 1) % 4;
}

void BraidsMachine::timerReschedule() {
  if (tim1Cr1_ & 1) {
    tim1NextUpdate_ = cpu_.cycles() + static_cast<uint64_t>(tim1Psc_ + 1) * (tim1Arr_ + 1);
    fallbackNext_ = kNever;
    cpu_.setExternalDeadline(tim1NextUpdate_);
  } else {
    tim1NextUpdate_ = kNever;
    fallbackNext_ = cpu_.cycles() + kFallbackSamplePeriod;
    cpu_.setExternalDeadline(fallbackNext_);
  }
}

// --- Broches -------------------------------------------------------------------------

uint32_t BraidsMachine::gpioInput(int port) const {
  uint32_t v = gpioOdr_[port] & 0xffff;
  if (port == 0) {
    // PA4 : l'entrée TRIG passe par un transistor, niveau haut = broche à 0.
    v = inputs_.trig ? (v & ~0x10u) : (v | 0x10u);
  } else if (port == 1) {
    v &= ~0x200u;  // PB9 à la masse : carte où FINE est lu par l'ADC interne
  } else {
    v &= ~0xe000u;
    if (!encoderPressed_) v |= 0x2000;
    if (encoderA_) v |= 0x4000;
    if (encoderB_) v |= 0x8000;
  }
  return v;
}

void BraidsMachine::gpioWrite(int port, uint32_t setMask, uint32_t clearMask) {
  const uint32_t before = gpioOdr_[port];
  const uint32_t after = ((before & ~clearMask) | setMask) & 0xffff;
  gpioOdr_[port] = after;
  if (port != 1 || before == after) return;
  const uint32_t rising = after & ~before;
  const uint32_t falling = before & ~after;

  if (falling & 0x0001) adcByteIndex_ = 0;   // PB0 : sélection de l'ADC
  if (falling & 0x1000) dacWordIndex_ = 0;   // PB12 : sélection du DAC

  // Afficheur : PB11 horloge, PB1 donnée, PB10 verrou, PB5..8 caractère allumé.
  if (rising & 0x0800) shiftRegister_ = static_cast<uint16_t>((shiftRegister_ >> 1) | ((after & 2) ? 0x8000 : 0));
  if (rising & 0x0400) latched_ = shiftRegister_;
  const bool wasLit = before & 0x1e0, isLit = after & 0x1e0;
  for (int i = 0; i < 4; ++i) {
    if (rising & (0x20u << i)) segments_[i] = latched_;
  }
  const uint64_t now = cpu_.cycles();
  if (isLit && !wasLit) litSince_ = now;
  if (!isLit && wasLit) litCycles_ += now - litSince_;
}

// --- Registres -----------------------------------------------------------------------

uint32_t BraidsMachine::read(uint32_t address, int size) {
  if (address >= 0x1ffff7e8u && address < 0x1ffff7f4u) {
    const uint32_t word = uid_[(address - 0x1ffff7e8u) >> 2] >> (8 * (address & 3));
    return size == 4 ? word : (size == 2 ? word & 0xffff : word & 0xff);
  }
  if (address == 0x1ffff7e0u) return kFlashSize / 1024;
  const uint32_t word = readRegister(address & ~3u) >> (8 * (address & 3));
  return size == 4 ? word : (size == 2 ? word & 0xffff : word & 0xff);
}

uint32_t BraidsMachine::readRegister(uint32_t a) {
  switch (a) {
    case kRcc: {
      uint32_t v = rccCr_ | 0x2;
      if (rccCr_ & (1u << 16)) v |= 1u << 17;   // quartz prêt
      if (rccCr_ & (1u << 24)) v |= 1u << 25;   // PLL verrouillée
      return v;
    }
    case kRcc + 4: return (rccCfgr_ & ~0xcu) | ((rccCfgr_ & 3) << 2);

    case kGpioA + 0x08: return gpioInput(0);
    case kGpioB + 0x08: return gpioInput(1);
    case kGpioC + 0x08: return gpioInput(2);
    case kGpioA + 0x0c: return gpioOdr_[0];
    case kGpioB + 0x0c: return gpioOdr_[1];
    case kGpioC + 0x0c: return gpioOdr_[2];

    case kTim1 + 0x00: return tim1Cr1_;
    case kTim1 + 0x0c: return tim1Dier_;
    case kTim1 + 0x10: return tim1Sr_;
    case kTim1 + 0x28: return tim1Psc_;
    case kTim1 + 0x2c: return tim1Arr_;

    case kSpi1 + 0x08: return 0x3;        // RXNE + TXE : transfert toujours terminé
    case kSpi1 + 0x0c: return spi1Dr_;
    case kSpi2 + 0x08: return 0x2;

    case kAdc1 + 0x00: return 0x2;        // fin de conversion
    case kAdc1 + 0x08: return adc1Cr2_;
    case kAdc1 + 0x4c: return inputs_.fine;
    case kDma1 + 0x08: return dmaCcr_;
    case kDma1 + 0x14: return dmaCmar_;

    case kFlashReg + 0x0c: return 0x20;   // opération terminée, jamais occupé
    case kFlashReg + 0x10: return flashCr_;
    case kFlashReg + 0x14: return flashAr_;
    default: {
      const auto it = otherRegisters_.find(a);
      return it == otherRegisters_.end() ? 0 : it->second;
    }
  }
}

void BraidsMachine::write(uint32_t address, uint32_t value, int size) {
  if (address >= CortexM3::kFlashBase && address < CortexM3::kFlashBase + kFlashSize) {
    flashProgram(address, value, size);
    return;
  }
  writeRegister(address & ~3u, value << (8 * (address & 3)));
}

void BraidsMachine::flashProgram(uint32_t address, uint32_t value, int size) {
  if (!(flashCr_ & 1) || (flashCr_ & 0x80)) return;   // PG absent ou flash verrouillée
  uint8_t* p = cpu_.flash() + (address - CortexM3::kFlashBase);
  for (int i = 0; i < size; ++i) p[i] &= static_cast<uint8_t>(value >> (8 * i));  // 1 -> 0 seulement
  ++flashWrites_;
  cpu_.stall(kFlashProgramCycles * static_cast<uint64_t>((size + 1) / 2));
}

void BraidsMachine::writeRegister(uint32_t a, uint32_t v) {
  switch (a) {
    case kRcc: rccCr_ = v; break;
    case kRcc + 4: rccCfgr_ = v; break;

    case kGpioA + 0x0c: gpioWrite(0, v & 0xffff, 0xffff); break;
    case kGpioB + 0x0c: gpioWrite(1, v & 0xffff, 0xffff); break;
    case kGpioC + 0x0c: gpioWrite(2, v & 0xffff, 0xffff); break;
    case kGpioA + 0x10: gpioWrite(0, v & 0xffff, v >> 16); break;
    case kGpioB + 0x10: gpioWrite(1, v & 0xffff, v >> 16); break;
    case kGpioC + 0x10: gpioWrite(2, v & 0xffff, v >> 16); break;
    case kGpioA + 0x14: gpioWrite(0, 0, v & 0xffff); break;
    case kGpioB + 0x14: gpioWrite(1, 0, v & 0xffff); break;
    case kGpioC + 0x14: gpioWrite(2, 0, v & 0xffff); break;

    case kTim1 + 0x00: {
      const bool was = tim1Cr1_ & 1;
      tim1Cr1_ = v & 0xffff;
      if (was != ((v & 1) != 0)) timerReschedule();
      break;
    }
    case kTim1 + 0x0c:
      tim1Dier_ = v & 0xffff;
      if ((tim1Dier_ & 1) && (tim1Sr_ & 1)) cpu_.setIrqPending(kTim1UpIrq, true);
      break;
    case kTim1 + 0x10: tim1Sr_ &= v; break;
    case kTim1 + 0x14:
      if (v & 1) { tim1Sr_ |= 1; if (tim1Dier_ & 1) cpu_.setIrqPending(kTim1UpIrq, true); }
      break;
    case kTim1 + 0x28: tim1Psc_ = v & 0xffff; if (tim1Cr1_ & 1) timerReschedule(); break;
    case kTim1 + 0x2c: tim1Arr_ = v & 0xffff; if (tim1Cr1_ & 1) timerReschedule(); break;

    case kSpi1 + 0x0c:
      // MCP3204 : 1er octet = début de trame, 2e = canal (la tension est
      // échantillonnée à ce moment), 3e = fin de la conversion.
      if (adcByteIndex_ == 0) {
        adcFirstByte_ = v;
        spi1Dr_ = 0xff;
      } else if (adcByteIndex_ == 1) {
        const uint32_t channel = (((adcFirstByte_ & 1) << 2) | ((v >> 6) & 3)) & 3;
        adcSample_ = inputs_.adc[channel] & 0xfff;
        spi1Dr_ = 0xe0 | (adcSample_ >> 8);
      } else {
        spi1Dr_ = adcSample_ & 0xff;
      }
      ++adcByteIndex_;
      break;

    case kSpi2 + 0x0c:
      // DAC : trame de 24 bits (commande, poids fort, poids faible) envoyée en deux mots.
      if (dacWordIndex_ == 0) dacHigh_ = v & 0xff;
      else if (dacWordIndex_ == 1) dacCode_ = static_cast<uint16_t>((dacHigh_ << 8) | ((v >> 8) & 0xff));
      ++dacWordIndex_;
      break;

    case kAdc1 + 0x08: adc1Cr2_ = v & ~0xcu; break;   // calibrations terminées aussitôt
    case kDma1 + 0x08: dmaCcr_ = v; break;
    case kDma1 + 0x14: dmaCmar_ = v; break;

    case kFlashReg + 0x00:
      // FLASH_ACR : cycles d'attente (bits 2..0) et lecture anticipée (bit 4).
      otherRegisters_[a] = v;
      cpu_.setFlashTiming(flashWaitOverride_ >= 0 ? static_cast<uint32_t>(flashWaitOverride_) : (v & 7), v & 0x10);
      break;
    case kFlashReg + 0x04: if (v == 0xcdef89abu) flashCr_ &= ~0x80u; break;
    case kFlashReg + 0x0c: break;
    case kFlashReg + 0x10:
      flashCr_ = v & ~0x40u;
      if ((v & 0x40) && (v & 0x02) && !(v & 0x80)) {   // effacement d'une page de 1 Ko
        const uint32_t page = (flashAr_ - CortexM3::kFlashBase) & ~0x3ffu;
        if (page < kFlashSize) {
          std::memset(cpu_.flash() + page, 0xff, 1024);
          ++flashWrites_;
          cpu_.stall(kFlashEraseCycles);
        }
      }
      break;
    case kFlashReg + 0x14: flashAr_ = v; break;

    default: otherRegisters_[a] = v; break;
  }
}

}  // namespace emu
}  // namespace tresses
