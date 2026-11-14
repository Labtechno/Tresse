// Tresses — état du module virtuel (broches, convertisseurs, flash).
//
// Ce fichier et tous les fichiers de shim/ sont inclus À L'INTÉRIEUR de l'espace
// de noms d'un "slot" (tresses_slotN) : chaque instance du plugin a donc sa propre
// copie de toutes ces variables, exactement comme chaque module a sa propre puce.
//
// Ils remplacent les drivers de braids/drivers/ et quelques en-têtes de
// stmlib/system/ en gardant leur interface publique à l'identique : le code du
// firmware (braids.cc, ui.cc, settings.cc, DSP) est compilé sans modification.

#ifndef TRESSES_VIRTUAL_HARDWARE_H_
#define TRESSES_VIRTUAL_HARDWARE_H_

// STM32F103 "medium density" : pages de flash de 1 Ko.
#ifndef PAGE_SIZE
#define PAGE_SIZE (uint16_t)0x400
#endif

namespace tresses_hw_detail {

// Les 16 derniers Ko des 128 Ko de flash du STM32F103CB : c'est là que le
// firmware range ses réglages (Storage<0x8020000, 4>).
const uint32_t kFlashEmulatedEnd = 0x8020000;
const uint32_t kFlashEmulatedSize = 16 * 1024;
const uint32_t kFlashEmulatedStart = kFlashEmulatedEnd - kFlashEmulatedSize;

struct VirtualHardware {
  // --- Entrées, écrites par l'émulateur avant chaque échantillon -----------------
  uint16_t adc_code[4];     // codes 12 bits présents sur l'ADC externe (MCP3204)
  uint16_t internal_adc;    // registre 16 bits (12 bits alignés à gauche) : potard FINE
  bool trig_high;           // tension présente sur le jack TRIG
  bool encoder_switch_low;  // bouton de l'encodeur enfoncé (broche tirée à 0)
  int encoder_pending;      // crans d'encodeur restant à jouer (+ horaire)
  uint8_t encoder_phase;    // position dans la séquence quadrature en cours
  int8_t encoder_direction;
  bool encoder_pin_a;
  bool encoder_pin_b;

  // --- Sorties ---------------------------------------------------------------------
  uint16_t dac_code;        // dernier mot envoyé au DAC 16 bits
  const char* display_buffer;  // les 4 caractères de l'afficheur
  const uint16_t* display_brightness;
  bool debug_pin;

  // --- Mémoire flash (fin de la flash du STM32) ---------------------------------------
  uint8_t flash[kFlashEmulatedSize];
  uint32_t flash_version;   // incrémenté à chaque écriture/effacement

  // --- Identifiant unique de la puce (sert à la "signature" SIGN) ---------------------
  uint32_t uid[3];
};

}  // namespace tresses_hw_detail

inline tresses_hw_detail::VirtualHardware tresses_hw;

inline uint8_t* tresses_flash_ptr(uint32_t address) {
  using namespace tresses_hw_detail;
  if (address < kFlashEmulatedStart || address >= kFlashEmulatedEnd) {
    return nullptr;
  }
  return &tresses_hw.flash[address - kFlashEmulatedStart];
}

inline void tresses_flash_erase_page(uint32_t address) {
  address -= address % PAGE_SIZE;
  uint8_t* p = tresses_flash_ptr(address);
  if (p) {
    std::memset(p, 0xff, PAGE_SIZE);
    ++tresses_hw.flash_version;
  }
}

inline void tresses_flash_program(uint32_t address, const void* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    uint8_t* p = tresses_flash_ptr(address + static_cast<uint32_t>(i));
    if (p) {
      // Comme une vraie flash : la programmation ne peut que faire passer des bits à 0.
      *p &= static_cast<const uint8_t*>(data)[i];
    }
  }
  ++tresses_hw.flash_version;
}

#endif  // TRESSES_VIRTUAL_HARDWARE_H_
