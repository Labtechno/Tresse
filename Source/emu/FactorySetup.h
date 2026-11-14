// Tresses — réglage d'usine d'un firmware : la procédure CAL du module, exécutée
// en actionnant l'encodeur et en lisant l'afficheur, comme le ferait un technicien.

#pragma once

#include "BraidsMachine.h"

namespace tresses {
namespace emu {

enum class FactoryResult {
  kCalibrated,     // procédure CAL menée au bout
  kNoCalMenu,      // le firmware a démarré, mais n'a pas d'entrée CAL. reconnaissable
  kNotRunning,     // le firmware n'a rien affiché ou s'est arrêté
};

// `idle` : entrées au repos (0 V sur V/OCT, COARSE et FINE au milieu, rien sur FM).
// `code1V`, `code3V` : codes de l'ADC pour 1 V et 3 V sur l'entrée V/OCT.
// La machine doit venir d'être mise sous tension ; elle est laissée en marche.
FactoryResult factorySetup(BraidsMachine& machine, const BraidsMachine::Inputs& idle,
                           uint16_t code1V, uint16_t code3V);

}  // namespace emu
}  // namespace tresses
