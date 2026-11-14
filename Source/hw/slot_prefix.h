// Tresses — début de chaque unité de compilation d'un slot.
//
// Tous les en-têtes système qu'utilise le firmware sont inclus ICI, en dehors de
// l'espace de noms du slot. Quand le firmware les inclut à nouveau (à
// l'intérieur de l'espace de noms), leurs gardes d'inclusion les ignorent.
// Si ton firmware utilise un autre en-tête système, ajoute-le à cette liste.

#include <algorithm>
#include <atomic>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <limits.h>
#include <limits>
#include <math.h>
#include <new>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <utility>

#include "FirmwareSlot.h"

// Les fonctions d'interruption sont déclarées extern "C" dans braids.cc : sans
// renommage, celles des différents slots porteraient le même nom à l'édition de
// liens. TRESSES_SLOT_RENAME est défini par l'unité générée.
#define main TRESSES_SLOT_RENAME(firmware_main)
#define NMI_Handler TRESSES_SLOT_RENAME(NMI_Handler)
#define HardFault_Handler TRESSES_SLOT_RENAME(HardFault_Handler)
#define MemManage_Handler TRESSES_SLOT_RENAME(MemManage_Handler)
#define BusFault_Handler TRESSES_SLOT_RENAME(BusFault_Handler)
#define UsageFault_Handler TRESSES_SLOT_RENAME(UsageFault_Handler)
#define SVC_Handler TRESSES_SLOT_RENAME(SVC_Handler)
#define DebugMon_Handler TRESSES_SLOT_RENAME(DebugMon_Handler)
#define PendSV_Handler TRESSES_SLOT_RENAME(PendSV_Handler)
#define SysTick_Handler TRESSES_SLOT_RENAME(SysTick_Handler)
#define TIM1_UP_IRQHandler TRESSES_SLOT_RENAME(TIM1_UP_IRQHandler)
#define TIM2_IRQHandler TRESSES_SLOT_RENAME(TIM2_IRQHandler)
#define TIM3_IRQHandler TRESSES_SLOT_RENAME(TIM3_IRQHandler)
#define TIM4_IRQHandler TRESSES_SLOT_RENAME(TIM4_IRQHandler)
#define USART1_IRQHandler TRESSES_SLOT_RENAME(USART1_IRQHandler)
#define EXTI0_IRQHandler TRESSES_SLOT_RENAME(EXTI0_IRQHandler)
#define EXTI15_10_IRQHandler TRESSES_SLOT_RENAME(EXTI15_10_IRQHandler)
