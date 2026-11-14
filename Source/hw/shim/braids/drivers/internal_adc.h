// Tresses — ADC interne virtuel (remplace braids/drivers/internal_adc.h).
//
// Sur le module, le potard FINE est lu en continu par l'ADC du STM32 et copié
// par DMA dans value_. Ici, value_ est relu à chaque appel depuis la broche
// virtuelle ; le filtrage est celui du firmware.

#ifndef BRAIDS_DRIVERS_INTERNAL_ADC_H_
#define BRAIDS_DRIVERS_INTERNAL_ADC_H_

#include "stmlib/stmlib.h"
#include "tresses_virtual_hardware.h"

namespace braids {

class InternalAdc {
 public:
  InternalAdc() { }
  ~InternalAdc() { }

  void Init() {
    // Broche PB9 tirée à la masse sur les cartes qui lisent FINE en interne.
    disabled_ = false;
    state_ = 0;
    value_ = tresses_hw.internal_adc;
  }

  inline int32_t value() {
    value_ = tresses_hw.internal_adc;  // copie DMA
    if (disabled_) {
      return 0;
    } else {
      int32_t v = (static_cast<int32_t>(value_) - 32768) << 8;
      int32_t delta = v - state_;
      state_ += (delta >> 8);
      return state_ >> 8;
    }
  }

 private:
  bool disabled_;
  uint16_t value_;
  int32_t state_;

  DISALLOW_COPY_AND_ASSIGN(InternalAdc);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_INTERNAL_ADC_H_
