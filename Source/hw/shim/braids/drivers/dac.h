// Tresses — driver DAC virtuel (remplace braids/drivers/dac.h).
// Le mot 16 bits envoyé en SPI au DAC est simplement capturé.

#ifndef BRAIDS_DRIVERS_DAC_H_
#define BRAIDS_DRIVERS_DAC_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

class Dac {
 public:
  Dac() { }
  ~Dac() { }

  void Init() { tresses_hw.dac_code = 32768; }

  inline void Write(uint16_t value) {
    tresses_hw.dac_code = value;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Dac);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_DAC_H_
