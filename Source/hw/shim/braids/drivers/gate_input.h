// Tresses — entrée TRIG virtuelle (remplace braids/drivers/gate_input.h).
// Sur le module, l'entrée passe par un transistor : la broche lit 0 quand le
// jack est à l'état haut. Détection de fronts identique à l'original.

#ifndef BRAIDS_DRIVERS_GATE_INPUT_H_
#define BRAIDS_DRIVERS_GATE_INPUT_H_

#include <stm32f10x_conf.h>
#include "stmlib/stmlib.h"

namespace braids {

class GateInput {
 public:
  GateInput() { }
  ~GateInput() { }

  void Init() {
    previous_state_ = true;
  }

  inline bool Read() {
    return tresses_hw.trig_high;
  }

  inline bool raised() {
    bool new_state = Read();
    bool rising_edge = (!previous_state_ && new_state);
    previous_state_ = new_state;
    return rising_edge;
  }

  inline bool lowered() {
    bool new_state = Read();
    bool falling_edge = (previous_state_ && !new_state);
    previous_state_ = new_state;
    return falling_edge;
  }

  inline bool state() { return previous_state_; }

 private:
  bool previous_state_;

  DISALLOW_COPY_AND_ASSIGN(GateInput);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_GATE_INPUT_H_
