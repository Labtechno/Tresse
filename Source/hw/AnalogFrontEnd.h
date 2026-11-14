// Tresses — étage analogique virtuel du module.
//
// Convertit la position des potards et les tensions des jacks en ce que les
// convertisseurs du STM32 lisent réellement :
//
//  - ADC externe 12 bits (MCP3204), 4 canaux :
//      0 : potard TIMBRE + CV TIMBRE x atténuverseur MODULATION (0..5 V = course du potard)
//      1 : potard COLOR + CV COLOR (0..5 V = course du potard)
//      2 : V/OCT + COARSE. Fenêtre d'environ -4 V à +6 V sur 4096 codes : c'est
//          la fenêtre qu'implique le firmware (code 1638 = C3 en mode FREE).
//          408 codes/V (la valeur par défaut du firmware en suppose 408,5) :
//          34 codes par demi-ton exactement, si bien qu'après la calibration
//          CAL du firmware chaque note tombe juste. COARSE ajoute ±4 V.
//      3 : CV FM x atténuverseur FM, centré sur le code 2048 (±5 V = pleine échelle).
//  - ADC interne du STM32 : potard FINE (±1 demi-ton dans le firmware).
//
// Hypothèses (non documentées par Mutable) : la sensibilité de l'entrée FM
// (±5 V à pleine échelle) et l'absence de bruit de conversion.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "FirmwareSlot.h"

namespace tresses {

// Position des potards.
struct PanelState {
  float fine = 0.0f;        // -1..+1 (midi = 0)
  float coarse = 0.0f;      // -1..+1 (midi = 0)  -> ±4 V
  float fm = 0.0f;          // atténuverseur FM, -1..+1
  float timbre = 0.5f;      // 0..1
  float modulation = 0.0f;  // atténuverseur de la CV TIMBRE, -1..+1
  float color = 0.5f;       // 0..1
};

// Tensions présentes sur les jacks d'entrée.
struct JackState {
  bool trig = false;
  float voct = 0.0f;    // volts
  float fm = 0.0f;      // volts
  float timbre = 0.0f;  // volts
  float color = 0.0f;   // volts
};

namespace analog {

constexpr double kVoctCodesPerVolt = 408.0;
constexpr double kVoctZeroCode = 1638.4;   // 0 V + COARSE au milieu
constexpr double kFmCodesPerVolt = 409.6;
constexpr double kFmZeroCode = 2048.0;
constexpr double kCoarseVolts = 4.0;
constexpr double kParameterCvFullScale = 5.0;

inline uint16_t clampCode(double code) {
  return static_cast<uint16_t>(std::clamp(std::lround(code), 0L, 4095L));
}

inline uint16_t voctCode(double volts, double coarse) {
  return clampCode(kVoctZeroCode + kVoctCodesPerVolt * (volts + coarse * kCoarseVolts));
}

inline FrameInput frame(const PanelState& p, const JackState& j) {
  FrameInput f {};
  const double timbre = p.timbre + p.modulation * j.timbre / kParameterCvFullScale;
  const double color = p.color + j.color / kParameterCvFullScale;
  f.adc[0] = clampCode(4095.0 * std::clamp(timbre, 0.0, 1.0));
  f.adc[1] = clampCode(4095.0 * std::clamp(color, 0.0, 1.0));
  f.adc[2] = voctCode(j.voct, p.coarse);
  f.adc[3] = clampCode(kFmZeroCode + kFmCodesPerVolt * p.fm * j.fm);
  const uint16_t fine12 = clampCode(2048.0 + 2047.0 * std::clamp<double>(p.fine, -1.0, 1.0));
  f.fine = static_cast<uint16_t>(fine12 << 4);  // aligné à gauche, comme le registre du STM32
  f.trig = j.trig ? 1 : 0;
  return f;
}

// Codes lus pendant la calibration d'usine : 1 V puis 3 V sur V/OCT avec
// COARSE et FINE au milieu, rien sur FM.
inline void factoryCalibrationCodes(uint16_t codes[3]) {
  codes[0] = voctCode(1.0, 0.0);
  codes[1] = voctCode(3.0, 0.0);
  codes[2] = clampCode(kFmZeroCode);
}

}  // namespace analog

}  // namespace tresses
