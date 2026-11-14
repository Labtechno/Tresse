// Tresses — afficheur virtuel (remplace braids/drivers/display.h).
//
// Même interface et même logique de luminosité (PWM sur 4 cycles) que le driver
// d'origine. Les 4 caractères sont lus par l'interface du plugin, qui les dessine
// avec la police 14 segments du firmware (chr_characters).

#ifndef BRAIDS_DRIVERS_DISPLAY_H_
#define BRAIDS_DRIVERS_DISPLAY_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

const uint8_t kDisplayWidth = 4;
const uint8_t kBrightnessLevels = 4;

class Display {
 public:
  Display() { }
  ~Display() { }

  void Init() {
    active_position_ = 0;
    brightness_pwm_cycle_ = 0;
    brightness_ = 3;
    memset(buffer_, ' ', kDisplayWidth);
    tresses_hw.display_buffer = buffer_;
    tresses_hw.display_brightness = &brightness_;
  }

  void Refresh() {
    if (brightness_pwm_cycle_ <= brightness_) {
      active_position_ = (active_position_ + 1) % kDisplayWidth;
    }
    brightness_pwm_cycle_ = (brightness_pwm_cycle_ + 1) % kBrightnessLevels;
  }

  void Print(const char* s) {
    strncpy(buffer_, s, kDisplayWidth);
  }

  char* mutable_buffer() { return buffer_; }
  void set_brightness(uint16_t brightness) { brightness_ = brightness; }

 private:
  char buffer_[kDisplayWidth];
  uint16_t active_position_;
  uint16_t brightness_pwm_cycle_;
  uint16_t brightness_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_DISPLAY_H_
