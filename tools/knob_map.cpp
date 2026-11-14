// Carte d'influence des potards TIMBRE et COLOR, modèle par modèle.
//
// Pour chaque modèle, on tourne un potard de 0 à 100 % (l'autre au milieu), on
// enregistre le son à chaque position et on mesure à quel point il change :
// spectre en bandes de tiers d'octave et enveloppe de volume, en décibels.
// On en tire :
//   - la force de l'effet (somme des changements d'une position à la suivante,
//     moins le bruit propre aux modèles aléatoires) ;
//   - la zone utile : la partie de la course où se fait 80 % du changement.
//
// Le firmware utilisé est la référence de Source/hw (Braids 1.9 recompilé pour
// l'ordinateur), dont le son est identique à celui du firmware officiel émulé.
//
//   knob_map [fichier.csv]

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>
#include "AnalogFrontEnd.h"
#include "FirmwareSlot.h"

using namespace tresses;

namespace {

const char* kNames[47] = {
  "CSAW", "MORPH (/\\-_)", "SAW/SQUARE (//-_)", "FOLD", "BUZZ (uuuu)", "SUB- (carre + sub)",
  "SUB/ (scie + sub)", "SYN- (carre sync)", "SYN/ (scie sync)", "//x3 (3 scies)",
  "-_x3 (3 carres)", "/\\x3 (3 triangles)", "SIx3 (3 sinus)", "RING", "//// (essaim de scies)",
  "//uu (scie + filtre en peigne)", "TOY*", "ZLPF", "ZPKF", "ZBPF", "ZHPF", "VOSM", "VOWL",
  "VFOF", "HARM", "FM", "FBFM", "WTFM", "PLUK", "BOWD", "BLOW", "FLUT", "BELL", "DRUM",
  "KICK", "CYMB", "SNAR", "WTBL", "WMAP", "WLIN", "WTx4", "NOIS", "TWNQ", "CLKN", "CLOU",
  "PRTC", "QPSK" };

constexpr double kRate = 96000.0;
constexpr int kSettle = 96 * 30;     // 30 ms après avoir tourné le potard
constexpr int kLength = 96 * 250;    // 250 ms enregistrées après le trigger
constexpr int kFft = 4096;
constexpr int kSteps = 21;           // 0 %, 5 %, ... 100 %
constexpr double kFloor = -70.0;     // dB : en dessous, c'est du silence

struct Reference {
  const FirmwareSlotApi* api;
  PanelState panel;
  JackState jacks;
  Reference() {
    api = firmwareSlot(claimFirmwareSlot());
    uint32_t uid[3] = { 0x54726573, 0x73657321, 0x00000001 };
    uint16_t codes[3];
    analog::factoryCalibrationCodes(codes);
    api->power_on(nullptr, 0, uid, analog::frame(panel, jacks), true, codes);
  }
  void run(int n, float* out) {
    std::vector<FrameInput> in(static_cast<size_t>(n), analog::frame(panel, jacks));
    api->run(in.data(), out, n);
  }
  void ms(int milliseconds) {
    std::vector<float> tmp(static_cast<size_t>(96 * milliseconds));
    run(96 * milliseconds, tmp.data());
  }
  void turn(int steps) { api->encoder(steps, false); ms(20 + 9 * std::abs(steps)); }
};

void fft(std::vector<std::complex<double>>& a) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2.0 * M_PI / static_cast<double>(len);
    const std::complex<double> w(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len) {
      std::complex<double> u(1.0, 0.0);
      for (size_t k = 0; k < len / 2; ++k) {
        const auto x = a[i + k], y = a[i + k + len / 2] * u;
        a[i + k] = x + y;
        a[i + k + len / 2] = x - y;
        u *= w;
      }
    }
  }
}

// Empreinte d'un son : 28 bandes de tiers d'octave (31,5 Hz à 16 kHz) et
// 8 tranches d'enveloppe, en dB.
struct Print { std::vector<double> bands, env; };

Print fingerprint(const std::vector<float>& x) {
  std::vector<double> power(kFft / 2, 0.0);
  int frames = 0;
  for (size_t start = 0; start + kFft <= x.size(); start += kFft / 2, ++frames) {
    std::vector<std::complex<double>> a(kFft);
    for (int i = 0; i < kFft; ++i) {
      const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (kFft - 1));
      a[static_cast<size_t>(i)] = x[start + static_cast<size_t>(i)] * w;
    }
    fft(a);
    for (int k = 0; k < kFft / 2; ++k) power[static_cast<size_t>(k)] += std::norm(a[static_cast<size_t>(k)]);
  }
  Print p;
  for (int b = 0; b < 28; ++b) {
    const double fc = 31.5 * std::pow(2.0, b / 3.0);
    const int lo = std::max(1, static_cast<int>(fc / std::pow(2.0, 1.0 / 6.0) * kFft / kRate));
    const int hi = std::max(lo + 1, static_cast<int>(fc * std::pow(2.0, 1.0 / 6.0) * kFft / kRate));
    double e = 0.0;
    for (int k = lo; k < hi && k < kFft / 2; ++k) e += power[static_cast<size_t>(k)];
    e /= frames * (kFft * kFft / 8.0);
    p.bands.push_back(std::max(kFloor, 10.0 * std::log10(e + 1e-20)));
  }
  const size_t seg = x.size() / 8;
  for (int s = 0; s < 8; ++s) {
    double e = 0.0;
    for (size_t i = 0; i < seg; ++i) e += x[s * seg + i] * x[s * seg + i];
    p.env.push_back(std::max(kFloor, 10.0 * std::log10(e / seg + 1e-20)));
  }
  return p;
}

// Écart moyen entre deux sons, en dB.
double distance(const Print& a, const Print& b) {
  double s = 0.0, t = 0.0;
  for (size_t i = 0; i < a.bands.size(); ++i) s += std::abs(a.bands[i] - b.bands[i]);
  for (size_t i = 0; i < a.env.size(); ++i) t += std::abs(a.env[i] - b.env[i]);
  return 0.75 * s / a.bands.size() + 0.25 * t / a.env.size();
}

struct Sweep {
  double strength = 0.0;    // dB cumulés (bruit déduit)
  double extremes = 0.0;    // écart entre 0 % et 100 %
  double from = 0.0, to = 1.0;  // zone où se fait 80 % du changement
};

Print render(Reference& r, float voct) {
  r.jacks.voct = voct;
  r.ms(kSettle / 96);
  r.jacks.trig = true;
  std::vector<float> x(kLength);
  r.run(96 * 2, x.data());
  r.jacks.trig = false;
  r.run(kLength, x.data());
  return fingerprint(x);
}

Sweep sweep(Reference& r, float* knob) {
  std::vector<double> steps(kSteps - 1, 0.0);
  double extremes = 0.0;
  for (float voct : { 0.0f, 2.0f }) {   // do à 65 Hz et à 262 Hz
    std::vector<Print> prints;
    for (int i = 0; i < kSteps; ++i) {
      *knob = static_cast<float>(i) / (kSteps - 1);
      prints.push_back(render(r, voct));
    }
    // Bruit propre au modèle : deux rendus au même réglage.
    *knob = 0.5f;
    const double noise = distance(render(r, voct), render(r, voct));
    for (int i = 0; i + 1 < kSteps; ++i)
      steps[static_cast<size_t>(i)] += std::max(0.0, distance(prints[static_cast<size_t>(i)], prints[static_cast<size_t>(i) + 1]) - noise) / 2.0;
    extremes += std::max(0.0, distance(prints.front(), prints.back()) - noise) / 2.0;
  }
  *knob = 0.5f;
  Sweep s;
  for (double d : steps) s.strength += d;
  s.extremes = extremes;
  if (s.strength > 0.0) {
    double acc = 0.0;
    bool fromSet = false;
    for (int i = 0; i + 1 < kSteps; ++i) {
      const double before = acc;
      acc += steps[static_cast<size_t>(i)];
      const double a = static_cast<double>(i) / (kSteps - 1), b = static_cast<double>(i + 1) / (kSteps - 1);
      if (!fromSet && acc >= 0.1 * s.strength) {
        s.from = a + (b - a) * (0.1 * s.strength - before) / std::max(1e-9, acc - before);
        fromSet = true;
      }
      if (acc >= 0.9 * s.strength) {
        s.to = a + (b - a) * (0.9 * s.strength - before) / std::max(1e-9, acc - before);
        break;
      }
    }
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  Reference r;
  r.ms(600);
  FILE* csv = argc > 1 ? std::fopen(argv[1], "w") : nullptr;
  if (csv) std::fprintf(csv, "index;modele;timbre_force;timbre_extremes;timbre_de;timbre_a;color_force;color_extremes;color_de;color_a\n");
  for (int model = 0; model < 47; ++model) {
    r.panel.timbre = r.panel.color = 0.5f;
    const Sweep t = sweep(r, &r.panel.timbre);
    const Sweep c = sweep(r, &r.panel.color);
    std::printf("%-30s TIMBRE %5.1f dB (%4.1f) zone %3.0f-%3.0f %%   COLOR %5.1f dB (%4.1f) zone %3.0f-%3.0f %%\n",
                kNames[model], t.strength, t.extremes, 100 * t.from, 100 * t.to,
                c.strength, c.extremes, 100 * c.from, 100 * c.to);
    std::fflush(stdout);
    if (csv) std::fprintf(csv, "%d;%s;%.2f;%.2f;%.0f;%.0f;%.2f;%.2f;%.0f;%.0f\n", model, kNames[model],
                          t.strength, t.extremes, 100 * t.from, 100 * t.to,
                          c.strength, c.extremes, 100 * c.from, 100 * c.to);
    r.turn(1);
  }
  if (csv) std::fclose(csv);
  return 0;
}
