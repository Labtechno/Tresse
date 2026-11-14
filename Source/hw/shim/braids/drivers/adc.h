// Tresses — driver ADC virtuel (remplace braids/drivers/adc.h).
//
// Le module lit TIMBRE, COLOR, V/OCT et FM avec un ADC externe 12 bits (MCP3204)
// en SPI, une étape de transaction par échantillon audio : 3 étapes par canal,
// donc un balayage complet des 4 canaux tous les 12 échantillons. Le moment où
// chaque canal est échantillonné et celui où sa valeur devient visible pour le
// firmware sont reproduits à l'identique.

#ifndef BRAIDS_DRIVERS_ADC_H_
#define BRAIDS_DRIVERS_ADC_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

const size_t kNumChannels = 4;

class Adc {
 public:
  Adc() { }
  ~Adc() { }

  void Init(bool double_speed) {
    rx_word_ = 0;
    low_byte_ = 0;
    active_channel_ = 0;
    acquisition_stage_ = 0;
    memset(channels_, 0, sizeof(channels_));
  }

  inline bool PipelinedScan() {
    switch (acquisition_stage_) {
      case 0:
        // 3e octet reçu : bits B7..B0 de la conversion en cours.
        rx_word_ |= low_byte_;
        channels_[active_channel_] = rx_word_;
        active_channel_ = (active_channel_ + 1) % kNumChannels;
        acquisition_stage_ = 1;
        break;

      case 1:
        // Envoi du numéro de canal : le MCP3204 échantillonne l'entrée.
        acquisition_stage_ = 2;
        break;

      case 2:
        {
          // 2e octet reçu : bits B11..B8.
          const uint16_t code = tresses_hw.adc_code[active_channel_] & 0xfff;
          rx_word_ = (code >> 8) << 8;
          low_byte_ = code & 0xff;
          acquisition_stage_ = 0;
        }
        break;
    }
    return (active_channel_ == 0 && acquisition_stage_ == 1);
  }

  bool PipelinedRead(uint8_t channel) {
    switch (acquisition_stage_) {
      case 0:
        rx_word_ |= low_byte_;
        channels_[active_channel_] = rx_word_;
        active_channel_ = channel;
        acquisition_stage_ = 1;
        break;

      case 1:
        acquisition_stage_ = 2;
        break;

      case 2:
        {
          const uint16_t code = tresses_hw.adc_code[active_channel_ % kNumChannels] & 0xfff;
          rx_word_ = (code >> 8) << 8;
          low_byte_ = code & 0xff;
          acquisition_stage_ = 0;
        }
        break;
    }
    return acquisition_stage_ == 1;
  }

  uint16_t Read(uint8_t channel) {
    return tresses_hw.adc_code[channel % kNumChannels] & 0xfff;
  }

  uint16_t channel(uint8_t index) const { return channels_[index]; }

 private:
  uint16_t rx_word_;
  uint16_t low_byte_;
  size_t active_channel_;
  size_t acquisition_stage_;
  uint16_t channels_[kNumChannels];

  DISALLOW_COPY_AND_ASSIGN(Adc);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_ADC_H_
