// Tresses — initialisation système virtuelle (remplace braids/drivers/system.h).
// Horloges, timers et interruptions sont cadencés par l'émulateur :
// TIM1 à 96 kHz (F_CPU / (timer_period + 1)) et SysTick à 1 kHz.

#ifndef BRAIDS_DRIVERS_SYSTEM_H_
#define BRAIDS_DRIVERS_SYSTEM_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

class System {
 public:
  System() { }
  ~System() { }

  void Init(uint32_t timer_period, bool application) {
    timer_period_ = timer_period;
  }

  void StartTimers() { }

  uint32_t timer_period() const { return timer_period_; }

 private:
  uint32_t timer_period_;

  DISALLOW_COPY_AND_ASSIGN(System);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_SYSTEM_H_
