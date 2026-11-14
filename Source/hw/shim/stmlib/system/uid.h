// Tresses — identifiant unique virtuel (remplace stmlib/system/uid.h).
// Sur le module, il est lu à l'adresse 0x1FFFF7E8 de la puce. Le firmware s'en
// sert pour la "signature" (SIGN), propre à chaque exemplaire.

#ifndef STMLIB_SYSTEM_UID_H_
#define STMLIB_SYSTEM_UID_H_

#include "stmlib/stmlib.h"
#include "tresses_virtual_hardware.h"

namespace stmlib {

inline uint32_t GetUniqueId(uint8_t word) {
  return tresses_hw.uid[word % 3];
}

};  // namespace stmlib

#endif  // STMLIB_SYSTEM_UID_H_
