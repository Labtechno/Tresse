// Tresses — encodeur virtuel (remplace braids/drivers/encoder.h).
//
// Le driver d'origine lit 3 broches à chaque milliseconde (SysTick) : le bouton
// et les deux canaux A/B de l'encodeur quadrature. Ici, les broches sont
// générées : un cran = une séquence quadrature complète sur 4 lectures, et le
// bouton reste à 0 tant qu'il est enfoncé. L'anti-rebond et le décodage sont
// ceux du firmware, copiés à l'identique.

#ifndef BRAIDS_DRIVERS_ENCODER_H_
#define BRAIDS_DRIVERS_ENCODER_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

class Encoder {
 public:
  Encoder() { }
  ~Encoder() { }

  void Init() {
    switch_state_ = 0xff;
    quadrature_decoding_state_[0] = quadrature_decoding_state_[1] = 0xff;
    tresses_hw.encoder_pin_a = true;
    tresses_hw.encoder_pin_b = true;
    tresses_hw.encoder_phase = 0;
  }

  void Debounce() {
    AdvanceVirtualQuadrature();
    switch_state_ = (switch_state_ << 1) | \
        (tresses_hw.encoder_switch_low ? 0 : 1);
    quadrature_decoding_state_[0] = (quadrature_decoding_state_[0] << 1) | \
        (tresses_hw.encoder_pin_a ? 1 : 0);
    quadrature_decoding_state_[1] = (quadrature_decoding_state_[1] << 1) | \
        (tresses_hw.encoder_pin_b ? 1 : 0);
  }

  inline bool released() const {
    return switch_state_ == 0x7f;
  }

  inline bool just_pressed() const {
    return switch_state_ == 0x80;
  }

  inline bool pressed() const {
    return switch_state_ == 0x00;
  }

  inline bool pressed_immediate() const {
    return tresses_hw.encoder_switch_low;
  }

  inline int32_t increment() const {
    int32_t increment = 0;
    uint8_t a = quadrature_decoding_state_[0];
    uint8_t b = quadrature_decoding_state_[1];
    if ((a & 0x03) == 0x02 && (b & 0x03) == 0x00) {
      increment = -1;
    } else {
      if ((b & 0x03) == 0x02 && (a & 0x03) == 0x00) {
        increment = 1;
      }
    }
    return increment;
  }

 private:
  // Un cran horaire : A descend, puis B (A,B) = 01, 00, 10, 11.
  // Un cran anti-horaire : B descend, puis A : 10, 00, 01, 11.
  static void AdvanceVirtualQuadrature() {
    if (tresses_hw.encoder_phase == 0) {
      if (tresses_hw.encoder_pending == 0) return;
      tresses_hw.encoder_direction = tresses_hw.encoder_pending > 0 ? 1 : -1;
      tresses_hw.encoder_pending -= tresses_hw.encoder_direction;
    }
    const bool cw = tresses_hw.encoder_direction > 0;
    switch (tresses_hw.encoder_phase) {
      case 0:
        tresses_hw.encoder_pin_a = !cw;
        tresses_hw.encoder_pin_b = cw;
        break;
      case 1:
        tresses_hw.encoder_pin_a = false;
        tresses_hw.encoder_pin_b = false;
        break;
      case 2:
        tresses_hw.encoder_pin_a = cw;
        tresses_hw.encoder_pin_b = !cw;
        break;
      default:
        tresses_hw.encoder_pin_a = true;
        tresses_hw.encoder_pin_b = true;
        break;
    }
    tresses_hw.encoder_phase = (tresses_hw.encoder_phase + 1) % 4;
  }

  uint8_t switch_state_;
  uint8_t quadrature_decoding_state_[2];

  DISALLOW_COPY_AND_ASSIGN(Encoder);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_ENCODER_H_
