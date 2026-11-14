// Tresses — interface entre le plugin et un module virtuel ("slot").
//
// Chaque slot contient une copie complète du firmware Braids (toutes ses
// variables globales), compilée dans son propre espace de noms. Une instance du
// plugin réserve un slot pour toute sa durée de vie : plusieurs Tresses peuvent
// ainsi tourner en même temps, chacun avec sa propre mémoire.

#pragma once

#include <cstddef>
#include <cstdint>

namespace tresses {

constexpr int kModuleSampleRate = 96000;          // TIM1 : 72 MHz / 750
constexpr size_t kFlashImageSize = 16 * 1024;      // fin de la flash du STM32F103

// État des entrées du module pendant un échantillon à 96 kHz.
struct FrameInput {
  uint16_t adc[4];   // codes 12 bits : TIMBRE, COLOR, V/OCT (+COARSE), FM
  uint16_t fine;     // registre 16 bits de l'ADC interne (potard FINE)
  uint8_t trig;      // 1 = tension présente sur le jack TRIG
};

struct DisplayState {
  char text[4];
  uint8_t brightness;  // valeur interne du driver (1..3, 3 au démarrage)
};

struct FirmwareSlotApi {
  // Mise sous tension : charge l'image flash (ou une flash vierge), puis exécute
  // Init() du firmware. Si `factory_calibrate`, lance ensuite la procédure CAL
  // du firmware avec les codes ADC fournis (1 V, 3 V, FM au repos).
  void (*power_on)(const uint8_t* flash, size_t flash_size, const uint32_t uid[3],
                   const FrameInput& initial, bool factory_calibrate,
                   const uint16_t calibration_codes[3]);

  // Fait tourner le module pendant n échantillons : interruption TIM1 à chaque
  // échantillon, SysTick toutes les millisecondes, boucle principale entre deux.
  // `out` reçoit la tension de sortie normalisée (DAC, -1..+1).
  void (*run)(const FrameInput* in, float* out, int n);

  // Encodeur : crans à jouer (+ = horaire) et état du bouton.
  void (*encoder)(int steps, bool pressed);

  void (*display)(DisplayState* out);

  uint32_t (*flash_version)();
  void (*read_flash)(uint8_t* dst);  // kFlashImageSize octets

  // Réglages en RAM (braids::SettingsData), pour retrouver l'état exact du module.
  size_t (*settings_size)();
  void (*read_settings)(uint8_t* dst);
  void (*write_settings)(const uint8_t* src);

  // Fréquence de l'interruption audio, déduite de la configuration de TIM1.
  int (*sample_rate)();

  // Police 14 segments du firmware (chr_characters, 256 entrées).
  const uint16_t* font;
};

int numFirmwareSlots();
const FirmwareSlotApi* firmwareSlot(int index);

// Réservation d'un slot libre (-1 si toutes les instances sont utilisées).
int claimFirmwareSlot();
void releaseFirmwareSlot(int index);

}  // namespace tresses
