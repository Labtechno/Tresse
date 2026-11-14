// Tresses — réglage d'usine (voir FactorySetup.h). Licence MIT.

#include "FactorySetup.h"

#include <cstring>
#include <vector>

namespace tresses {
namespace emu {

namespace {

// Dessin des caractères utiles sur l'afficheur 14 segments (police de Braids,
// braids/resources/characters.py) : segments a..p = bits 15..2.
uint16_t glyph(char c) {
  auto bits = [](const char* segments) {
    uint16_t v = 0;
    for (const char* s = segments; *s; ++s) {
      static const char order[] = "abcdefghjklmnp";
      v |= static_cast<uint16_t>(0x8000u >> (std::strchr(order, *s) - order));
    }
    return v;
  };
  switch (c) {
    case 'C': return bits("afed");
    case 'A': return bits("afepkbc");
    case 'L': return bits("def");
    case '.': return bits("m");
    case '>': return bits("gn");
    case '2': return bits("abpked");
    case '4': return bits("fpkbc");
    default: return 0;
  }
}

struct Operator {
  BraidsMachine& machine;
  BraidsMachine::Inputs inputs;

  void ms(int milliseconds) {
    const int n = 96 * milliseconds;
    std::vector<BraidsMachine::Inputs> in(static_cast<size_t>(n), inputs);
    std::vector<float> out(static_cast<size_t>(n));
    machine.run(in.data(), out.data(), n);
  }
  bool shows(const char* text) {
    const auto d = machine.display();
    for (int i = 0; i < 4; ++i) if (d.segments[i] != glyph(text[i])) return false;
    return true;
  }
  bool blank() {
    const auto d = machine.display();
    return (d.segments[0] | d.segments[1] | d.segments[2] | d.segments[3]) == 0;
  }
  void turn(int steps) { machine.encoderTurn(steps); ms(30); }
  void click() { machine.encoderPress(true); ms(40); machine.encoderPress(false); ms(40); }
  void longPress() { machine.encoderPress(true); ms(1000); machine.encoderPress(false); ms(40); }
};

}  // namespace

FactoryResult factorySetup(BraidsMachine& machine, const BraidsMachine::Inputs& idle,
                           uint16_t code1V, uint16_t code3V) {
  Operator op { machine, idle };
  op.ms(800);  // animation de démarrage
  if (machine.crashed() || op.blank()) return FactoryResult::kNotRunning;

  op.click();  // ouvre le menu
  const auto top = machine.display();
  int turns = 0;
  while (!op.shows("CAL.") && turns < 90) { op.turn(1); ++turns; }
  if (!op.shows("CAL.")) {
    op.turn(-turns);
    op.click();
    return FactoryResult::kNoCalMenu;
  }
  op.longPress();
  if (!op.shows(">C2 ")) return FactoryResult::kNoCalMenu;
  op.inputs.adc[2] = code1V;
  op.ms(50);
  op.click();
  if (!op.shows(">C4 ")) return FactoryResult::kNoCalMenu;
  op.inputs.adc[2] = code3V;
  op.ms(50);
  op.click();
  op.inputs = idle;

  // Retour en haut du menu puis clic : le firmware sauvegarde et revient au modèle.
  for (int i = 0; i < turns + 4; ++i) {
    const auto d = machine.display();
    if (std::memcmp(d.segments, top.segments, sizeof(d.segments)) == 0) break;
    op.turn(-1);
  }
  op.click();
  op.ms(150);
  return machine.crashed() ? FactoryResult::kNotRunning : FactoryResult::kCalibrated;
}

}  // namespace emu
}  // namespace tresses
