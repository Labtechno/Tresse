// Tresses — remplace l'en-tête de la bibliothèque STM32 pour le module virtuel.
//
// Seuls les éléments utilisés en dehors des drivers sont fournis : le registre
// d'état du timer TIM1 (lu par l'interruption audio de braids.cc) et F_CPU.

#ifndef TRESSES_STM32F10X_CONF_SHIM_H_
#define TRESSES_STM32F10X_CONF_SHIM_H_

#include "tresses_virtual_hardware.h"

#ifndef F_CPU
#define F_CPU 72000000L
#endif

struct TressesTimerRegisters {
  volatile uint16_t SR;
};

inline TressesTimerRegisters tresses_tim1;

#define TIM1 (&tresses_tim1)
#define TIM_IT_Update ((uint16_t)0x0001)

#endif  // TRESSES_STM32F10X_CONF_SHIM_H_
