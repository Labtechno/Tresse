// Test décisif du processeur : le vrai code de synthèse de Braids (les 47 modèles),
// compilé pour Cortex-M3 comme dans le firmware, doit produire exactement les mêmes
// échantillons sur le processeur émulé que sur une référence.
#include <stdint.h>
#include <string.h>

#include "braids/macro_oscillator.h"
#include "braids/envelope.h"
#include "braids/quantizer.h"
#include "braids/quantizer_scales.h"
#include "stmlib/utils/random.h"

extern "C" void emit(uint32_t v);
#ifdef __arm__
#define MARK(v) (*(volatile uint32_t*) 0x4000c010 = (v))
#else
#define MARK(v) ((void) 0)
#endif

using namespace braids;

MacroOscillator osc;
Envelope envelope;
Quantizer quantizer;
static int16_t buffer[24];
static uint8_t sync_buffer[24];

int main(void) {
  const int kBlocks = 300;
  uint32_t total = 2166136261u;
  quantizer.Init();
  envelope.Init();
  for (int shape = 0; shape <= MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META; ++shape) {
    stmlib::Random::Seed(0x1234 + shape);
    memset(static_cast<void*>(&osc), 0, sizeof(osc));
    osc.Init();
    osc.set_shape(static_cast<MacroOscillatorShape>(shape));
    quantizer.Configure(scales[shape % 49]);
    uint32_t h = 2166136261u, lcg = 0xbeef + shape;
    MARK(0x100 + shape);
    for (int block = 0; block < kBlocks; ++block) {
      lcg = lcg * 1664525u + 1013904223u;
      // Balayages lents de TIMBRE et COLOR, hauteur par paliers, triggers réguliers.
      const int16_t timbre = (block * 109) & 0x7fff;
      const int16_t color = 32767 - ((block * 67) & 0x7fff);
      int32_t pitch = (36 << 7) + ((block / 25) * 7 * 128) + ((lcg >> 20) & 63);
      pitch = quantizer.Process(pitch, 60 << 7);
      if (block % 50 == 0) { osc.Strike(); envelope.Trigger(ENV_SEGMENT_ATTACK); }
      envelope.Update(block & 127, 127 - (block & 127));
      const uint32_t ad = envelope.Render();
      osc.set_parameters(timbre, color);
      osc.set_pitch(pitch + (ad >> 9));
      memset(sync_buffer, 0, sizeof(sync_buffer));
      if (block % 7 == 3) sync_buffer[(lcg >> 8) % 24] = 1;
      osc.Render(sync_buffer, buffer, 24);
      for (int i = 0; i < 24; ++i) h = (h ^ static_cast<uint16_t>(buffer[i])) * 16777619u;
    }
    MARK(0);
    emit(h);
    total = (total ^ h) * 16777619u;
  }
  emit(total);
  return 0;
}
