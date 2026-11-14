// Tresses — broche de debug virtuelle (remplace braids/drivers/debug_pin.h).
// Sur le module, elle sert à mesurer la durée de RenderBlock() à l'oscilloscope
// (#define PROFILE_RENDER dans braids.cc).

#ifndef BRAIDS_DRIVERS_DEBUG_PIN_H_
#define BRAIDS_DRIVERS_DEBUG_PIN_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

class DebugPin {
 public:
  DebugPin() { }
  ~DebugPin() { }

  void Init() { tresses_hw.debug_pin = false; }
  void High() { tresses_hw.debug_pin = true; }
  void Low() { tresses_hw.debug_pin = false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugPin);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_DEBUG_PIN_H_
