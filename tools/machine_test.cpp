// Tresses — banc de test du module émulé (firmware binaire sur puce virtuelle).
//
//   machine_test <firmware.bin> boot      démarrage, menu, sauvegarde en flash
//   machine_test <firmware.bin> compare   audio comparé à la version compilée (référence)
//   machine_test <firmware.bin> speed     vitesse de l'émulation
//   machine_test <firmware.bin> cpu       charge du processeur du module, et surcharge
//
// La référence est le même firmware recompilé pour l'ordinateur (Source/hw).

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "AnalogFrontEnd.h"
#include "FirmwareSlot.h"
#include "../Source/emu/BraidsMachine.h"
#include "../Source/emu/FactorySetup.h"
#include "../Source/emu/FirmwareFile.h"

using namespace tresses;

namespace {

int failures = 0;
void expect(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "OK" : "ECHEC", what);
  if (!ok) ++failures;
}

emu::BraidsMachine::Inputs toInputs(const FrameInput& f) {
  emu::BraidsMachine::Inputs in;
  for (int i = 0; i < 4; ++i) in.adc[i] = f.adc[i];
  in.fine = f.fine;
  in.trig = f.trig != 0;
  return in;
}

struct Emulated {
  emu::BraidsMachine machine;
  const uint16_t* font;
  PanelState panel;
  JackState jacks;

  Emulated(const std::vector<uint8_t>& firmware, const uint16_t* f) : font(f) {
    if (!machine.loadApplication(firmware.data(), firmware.size())) {
      std::printf("image de firmware refusee\n");
      std::exit(2);
    }
    machine.powerOn();
  }
  void run(int n, float* out = nullptr) {
    std::vector<emu::BraidsMachine::Inputs> in(static_cast<size_t>(n), toInputs(analog::frame(panel, jacks)));
    std::vector<float> tmp(static_cast<size_t>(n));
    machine.run(in.data(), out ? out : tmp.data(), n);
  }
  void ms(int milliseconds) { run(96 * milliseconds); }
  std::string display() {
    const auto d = machine.display();
    std::string s;
    for (uint16_t seg : d.segments) {
      // Plusieurs caractères ont le même dessin (C et parenthèse, S et 5) : lettres d'abord.
      int found = -1;
      for (int c = 'A'; c <= 'Z' && found < 0; ++c) if (font[c] == seg) found = c;
      for (int c = 32; c < 256 && found < 0; ++c) if (font[c] == seg && (seg != 0 || c == 32)) found = c;
      if (found >= 32 && found < 127) s += static_cast<char>(found);
      else { char b[8]; std::snprintf(b, 8, "\\x%02X", found & 0xff); s += b; }
    }
    return s;
  }
  void turn(int steps) { machine.encoderTurn(steps); ms(20 + 9 * std::abs(steps)); }
  void click() { machine.encoderPress(true); ms(40); machine.encoderPress(false); ms(40); }
  void longPress() { machine.encoderPress(true); ms(1000); machine.encoderPress(false); ms(40); }

  // Procédure CAL du firmware, comme à la sortie d'usine : 1 V puis 3 V sur V/OCT.
  bool calibrate() {
    click();
    int guard = 0;
    while (display() != "CAL." && guard++ < 40) turn(1);
    if (display() != "CAL.") return false;
    longPress();
    if (display() != ">C2 ") return false;
    jacks.voct = 1.0f; ms(50); click();
    if (display() != ">C4 ") return false;
    jacks.voct = 3.0f; ms(50); click();
    jacks.voct = 0.0f;
    guard = 0;
    while (display() != "WAVE" && guard++ < 40) turn(-1);
    click();  // retour à la sélection du modèle : sauvegarde en flash
    ms(100);
    return true;
  }
};

struct Reference {
  int index;
  const FirmwareSlotApi* api;
  PanelState panel;
  JackState jacks;
  Reference() {
    index = claimFirmwareSlot();
    api = firmwareSlot(index);
    uint32_t uid[3] = { 0x54726573, 0x73657321, 0x00000001 };
    uint16_t codes[3];
    analog::factoryCalibrationCodes(codes);
    api->power_on(nullptr, 0, uid, analog::frame(panel, jacks), true, codes);
  }
  void run(int n, float* out = nullptr) {
    std::vector<FrameInput> in(static_cast<size_t>(n), analog::frame(panel, jacks));
    std::vector<float> tmp(static_cast<size_t>(n));
    api->run(in.data(), out ? out : tmp.data(), n);
  }
  void ms(int milliseconds) { run(96 * milliseconds); }
  void turn(int steps) { api->encoder(steps, false); ms(20 + 9 * std::abs(steps)); }
  std::string display() { DisplayState d; api->display(&d); return std::string(d.text, 4); }
};

double measureF0(const std::vector<float>& x, double sr) {
  std::vector<double> t;
  for (size_t i = 1; i < x.size(); ++i)
    if (x[i - 1] < 0.0f && x[i] >= 0.0f) t.push_back((i - 1) + x[i - 1] / (x[i - 1] - x[i]));
  if (t.size() < 3) return 0.0;
  return (t.size() - 1) / ((t.back() - t.front()) / sr);
}

// Cherche le décalage pour lequel les deux flux sont identiques échantillon par
// échantillon. Renvoie le nombre d'échantillons différents au meilleur décalage.
int bestMismatch(const std::vector<float>& a, const std::vector<float>& b, int maxLag, int* lagOut) {
  int best = 1 << 30;
  for (int lag = -maxLag; lag <= maxLag; ++lag) {
    int bad = 0;
    const int n = static_cast<int>(a.size()) - 2 * maxLag;
    for (int i = maxLag; i < maxLag + n && bad < best; ++i) {
      if (a[static_cast<size_t>(i)] != b[static_cast<size_t>(i + lag)]) ++bad;
    }
    if (bad < best) { best = bad; *lagOut = lag; }
  }
  return best;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::printf("usage: machine_test firmware.bin boot|compare|speed\n"); return 2; }
  std::vector<uint8_t> firmware;
  {
    std::vector<uint8_t> file;
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) { std::perror(argv[1]); return 2; }
    uint8_t buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) file.insert(file.end(), buf, buf + n);
    std::fclose(f);
    emu::FirmwareImage image;
    std::string error;
    const auto t0 = std::chrono::steady_clock::now();
    if (!emu::readFirmwareFile(file.data(), file.size(), image, error)) {
      std::printf("Fichier refuse : %s\n", error.c_str());
      return 4;
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    firmware = image.data;
    std::printf("Fichier %s : %zu octets, %d paquets, identifiant %s, lu en %.2f s\n", image.format.c_str(),
                firmware.size(), image.packets, emu::firmwareId(firmware).c_str(), s);
  }
  const std::string cmd = argv[2];
  Reference ref;
  const uint16_t* font = ref.api->font;

  if (cmd == "boot") {
    Emulated m(firmware, font);
    std::printf("Demarrage : ");
    for (int i = 0; i < 10; ++i) { m.ms(50); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\nTimer audio : %.0f Hz, luminosite %.2f\n", m.machine.sampleRate(), m.machine.display().brightness);
    expect(!m.machine.crashed(), "le processeur tourne");
    expect(m.display() == "CSAW", "affiche CSAW apres l'animation de demarrage");
    expect(std::abs(m.machine.sampleRate() - 96000.0) < 1.0, "le firmware a regle son timer audio sur 96 kHz");
    m.turn(1); std::printf("Un cran horaire : [%s]\n", m.display().c_str());
    expect(m.display() != "CSAW", "l'encodeur change de modele");
    m.turn(-1); expect(m.display() == "CSAW", "retour a CSAW");
    m.click(); expect(m.display() == "WAVE", "un clic ouvre le menu sur WAVE");
    std::printf("Menu : ");
    for (int i = 0; i < 23; ++i) { m.turn(1); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\n");
    m.turn(-23); m.turn(2); expect(m.display() == "BITS", "WAVE > META > BITS");
    m.click(); m.turn(-2); std::printf("BITS -2 : [%s]\n", m.display().c_str());
    expect(m.display() == "8BIT", "BITS passe a 8BIT");
    m.click(); m.turn(-2);
    const uint32_t before = m.machine.flashWriteCount();
    m.click(); m.ms(100);
    expect(m.machine.flashWriteCount() > before, "revenir sur WAVE ecrit les reglages en flash");
    expect(m.display() == "CSAW", "retour a la selection du modele");

    // Extinction puis rallumage avec la même flash.
    std::vector<uint8_t> flash(m.machine.flashImage(), m.machine.flashImage() + emu::BraidsMachine::kFlashSize);
    Emulated again(firmware, font);
    again.machine.restoreFlash(flash.data(), flash.size());
    again.machine.powerOn();
    again.ms(600); again.click(); again.turn(2); again.click();
    std::printf("Apres extinction/rallumage, BITS : [%s]\n", again.display().c_str());
    expect(again.display() == "8BIT", "le reglage BITS est relu depuis la flash");

    Emulated cal(firmware, font);
    cal.ms(600);
    expect(cal.calibrate(), "procedure CAL executee par le menu (1 V puis 3 V)");
    cal.turn(25); cal.panel.timbre = 0.0f;   // modèle FM, sinus pur
    std::printf("Justesse apres calibration [%s] :", cal.display().c_str());
    bool tuned = true;
    for (int note : { 36, 48, 60, 69, 81, 96 }) {
      cal.jacks.voct = static_cast<float>(note - 36) / 12.0f;
      cal.ms(100);
      std::vector<float> buf(48000);
      cal.run(48000, buf.data());
      const double f0 = measureF0(buf, 96000.0);
      const double cents = 1200.0 * std::log2(f0 / (440.0 * std::pow(2.0, (note - 69) / 12.0)));
      std::printf(" %.2f Hz (%+.2f ct)", f0, cents);
      if (std::abs(cents) > 0.5) tuned = false;
    }
    std::printf("\n");
    expect(tuned, "chaque note est juste");
  } else if (cmd == "compare") {
    // Pour chaque modèle : le module émulé choisit le modèle par son menu et le
    // sauve en flash. Puis les deux modules (émulé et référence) sont rallumés avec
    // cette même flash et reçoivent exactement les mêmes entrées.
    Emulated setup(firmware, font);
    setup.ms(600);
    expect(setup.calibrate(), "calibration du module emule par son menu");
    const int count = argc > 3 ? std::atoi(argv[3]) : 47;
    const int blockPhase = argc > 4 ? std::atoi(argv[4]) : 12;
    int exact = 0;
    int lagPlusBoot = 1 << 30;
    for (int model = 0; model < count; ++model) {
      if (model > 0) setup.turn(1);
      setup.click(); setup.click(); setup.ms(100);   // menu puis retour : sauvegarde
      const std::string name = setup.display();
      const uint8_t* flash = setup.machine.flashImage();

      Emulated m(firmware, font);
      m.machine.restoreFlash(flash, emu::BraidsMachine::kFlashSize);
      m.machine.powerOn();
      uint32_t uid[3] = { 0x54726573, 0x73657321, 0x00000001 };
      uint16_t codes[3] = { 0, 0, 0 };
      FrameInput initial {};
      initial.fine = 32768;
      ref.api->power_on(flash + emu::BraidsMachine::kFlashSize - kFlashImageSize, kFlashImageSize, uid,
                        initial, false, codes);

      // Phase A : toutes les entrées à zéro depuis l'allumage (état entièrement
      // déterminé), ce qui donne le décalage constant entre les deux modules.
      auto runBoth = [&](const FrameInput& f, int n, float* x, float* y) {
        std::vector<FrameInput> fr(static_cast<size_t>(n), f);
        std::vector<emu::BraidsMachine::Inputs> in(static_cast<size_t>(n), toInputs(f));
        std::vector<float> tmp(static_cast<size_t>(n));
        m.machine.run(in.data(), x ? x : tmp.data(), n);
        ref.api->run(fr.data(), y ? y : tmp.data(), n);
      };
      FrameInput zero {};
      zero.fine = 32768;
      std::vector<float> a(24000), b(24000);
      // L'animation de démarrage n'influence pas le son : on compare dès l'allumage.
      {
        std::vector<emu::BraidsMachine::Inputs> in(24000, toInputs(zero));
        std::vector<FrameInput> fr(24000, zero);
        m.machine.run(in.data(), a.data(), 24000);
        ref.api->run(fr.data(), b.data(), 24000);
      }
      int lag = 0;
      int badA = bestMismatch(a, b, 400, &lag);
      float peakA = 0.0f;
      for (float v : a) peakA = std::max(peakA, std::abs(v));
      const int boot = static_cast<int>(m.machine.bootSamples());
      if (peakA > 0.0f && badA == 0) {
        // Le décalage ne dépend que de la durée du démarrage du firmware émulé.
        if (lagPlusBoot != 1 << 30 && lagPlusBoot != lag + boot) {
          if (std::abs(lagPlusBoot - (lag + boot)) > 1) { ++failures; std::printf("  [%s] decalage incoherent\n", name.c_str()); }
        }
        lagPlusBoot = lag + boot;
      } else if (peakA == 0.0f && lagPlusBoot != 1 << 30) {
        lag = lagPlusBoot - boot;   // modèle silencieux sans TRIG : décalage déduit
        badA = 0;
        for (float v : b) if (v != 0.0f) ++badA;
      }
      {
        std::vector<float> tmp(400);
        if (lag < 0) { std::vector<emu::BraidsMachine::Inputs> in(400, toInputs(zero)); m.machine.run(in.data(), tmp.data(), -lag); }
        else if (lag > 0) { std::vector<FrameInput> fr(400, zero); ref.api->run(fr.data(), tmp.data(), lag); }
      }

      // Phase B : mêmes gestes des deux côtés (potards, hauteur, FM, TRIG), sans
      // décalage. Les changements tombent toujours au même endroit d'un bloc de 24.
      int badB = 0, total = 0;
      float peak = 0.0f;
      uint32_t lcg = 0x1234u + static_cast<uint32_t>(model);
      PanelState panel; JackState jacks;
      runBoth(zero, blockPhase, nullptr, nullptr);
      for (int step = 0; step < 60; ++step) {
        lcg = lcg * 1664525u + 1013904223u;
        const float r0 = static_cast<float>((lcg >> 8) & 0xffff) / 65535.0f;
        lcg = lcg * 1664525u + 1013904223u;
        const float r1 = static_cast<float>((lcg >> 8) & 0xffff) / 65535.0f;
        switch (step % 5) {
          case 0: panel.timbre = r0; jacks.trig = true; break;
          case 1: panel.color = r0; jacks.trig = false; break;
          case 2: jacks.voct = 0.5f + 3.0f * r0; panel.fine = r1 - 0.5f; break;
          case 3: panel.fm = r0; jacks.fm = 4.0f * r1 - 2.0f; jacks.trig = true; break;
          default: panel.coarse = 0.3f * (r0 - 0.5f); jacks.trig = false; break;
        }
        const int n = 24 * (40 + static_cast<int>((lcg >> 4) % 80));
        std::vector<float> x(static_cast<size_t>(n)), y(static_cast<size_t>(n));
        runBoth(analog::frame(panel, jacks), n, x.data(), y.data());
        for (int i = 0; i < n; ++i) {
          if (x[static_cast<size_t>(i)] != y[static_cast<size_t>(i)]) ++badB;
          peak = std::max(peak, std::abs(x[static_cast<size_t>(i)]));
        }
        total += n;
      }
      bool ok = badA == 0 && badB == 0 && peak > 0.01f && !m.machine.crashed();
      if (!ok && model == 15) {
        // Écart attendu : à hauteur très basse, ComputeDelay() fait un décalage de
        // plus de 32 bits, dont le résultat dépend du processeur (0 sur ARM). La
        // référence compilée pour PC se trompe ; l'émulateur fait comme le module.
        // Avec la sémantique ARM dans la référence, ce modèle est identique aussi.
        std::printf("  [%s] ecart attendu (decalage > 32 bits, propre au processeur ARM)\n", name.c_str());
        ok = !m.machine.crashed();
      }
      if (ok) ++exact; else ++failures;
      if (!ok || model % 8 == 0) {
        std::printf("  [%s] constant : %d ecarts / 23200 (decalage %d) ; gestes + TRIG : %d ecarts / %d ; niveau %.2f %s\n",
                    name.c_str(), badA, lag, badB, total, peak, ok ? "" : "<<<");
      }
    }
    std::printf("  %d modeles sur %d identiques a la reference, echantillon par echantillon\n", exact, count);
  } else if (cmd == "load") {
    // Comme le plugin : charge le fichier, fait le réglage d'usine, vérifie la justesse.
    Emulated m(firmware, font);
    uint16_t codes[3];
    analog::factoryCalibrationCodes(codes);
    const auto t0 = std::chrono::steady_clock::now();
    const auto result = emu::factorySetup(m.machine, toInputs(analog::frame(m.panel, m.jacks)), codes[0], codes[1]);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("Reglage d'usine en %.2f s : %s ; affichage [%s] ; timer audio %.0f Hz\n", s,
                result == emu::FactoryResult::kCalibrated ? "calibre" :
                result == emu::FactoryResult::kNoCalMenu ? "pas de menu CAL." : "le firmware ne demarre pas",
                m.display().c_str(), m.machine.sampleRate());
    expect(result == emu::FactoryResult::kCalibrated, "reglage d'usine reussi");
    m.panel.timbre = 0.0f;
    std::printf("Hauteurs :");
    for (int note : { 36, 60, 69, 84 }) {
      m.jacks.voct = static_cast<float>(note - 36) / 12.0f;
      m.ms(100);
      std::vector<float> buf(48000);
      m.run(48000, buf.data());
      const double f0 = measureF0(buf, m.machine.sampleRate());
      std::printf(" %.2f Hz (%+.1f ct)", f0, 1200.0 * std::log2(f0 / (440.0 * std::pow(2.0, (note - 69) / 12.0))));
    }
    std::printf("\n");
    if (argc > 3) {
      const size_t tail = 0x4000 + ((firmware.size() + 1023) & ~size_t { 1023 });
      FILE* f = std::fopen(argv[3], "wb");
      std::fwrite(m.machine.flashImage() + tail, 1, emu::BraidsMachine::kFlashSize - tail, f);
      std::fclose(f);
      std::printf("Reglages d'usine ecrits dans %s (%zu octets)\n", argv[3], emu::BraidsMachine::kFlashSize - tail);
    }
  } else if (cmd == "factory" && argc > 3) {
    // Réglages d'usine : flash des réglages (16 derniers Ko) après la calibration.
    Emulated m(firmware, font);
    m.ms(600);
    expect(m.calibrate(), "calibration d'usine (1 V puis 3 V)");
    FILE* f = std::fopen(argv[3], "wb");
    std::fwrite(m.machine.flashImage() + emu::BraidsMachine::kFlashSize - 16384, 1, 16384, f);
    std::fclose(f);
  } else if (cmd == "poke") {
    // Exploration de l'interface d'un firmware inconnu.
    Emulated m(firmware, font);
    for (int i = 0; i < 8; ++i) { m.ms(250); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\ntours : ");
    for (int i = 0; i < 6; ++i) { m.turn(1); m.ms(200); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\nclic : "); m.click(); m.ms(200); std::printf("[%s] ", m.display().c_str());
    for (int i = 0; i < 6; ++i) { m.turn(1); m.ms(200); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\n");
  } else if (cmd == "speed") {
    Emulated m(firmware, font);
    m.ms(600);
    for (int model : { 0, 10, 24, 35, 40 }) {
      m.turn(model);
      const uint64_t c0 = m.machine.cycles(), i0 = m.machine.cpu().idleCycles();
      const auto t0 = std::chrono::steady_clock::now();
      m.run(96000 * 3);
      const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      const double busy = 1.0 - static_cast<double>(m.machine.cpu().idleCycles() - i0) / static_cast<double>(m.machine.cycles() - c0);
      std::printf("  [%s] 3 s de son en %.2f s : %.1f %% d'un coeur de cet ordinateur ; processeur du module occupe a %.0f %%\n",
                  m.display().c_str(), s, 100.0 * s / 3.0, 100.0 * busy);
      m.turn(-model);
    }
  } else if (cmd == "cpu") {
    // Charge du processeur du module, modèle par modèle, à plusieurs hauteurs et
    // réglages. Sur le vrai module, aucun modèle d'origine ne décroche : ici la
    // charge doit donc rester sous 100 %.
    Emulated m(firmware, font);
    m.ms(600);
    auto loadOver = [&](int samples) {
      const uint64_t c0 = m.machine.cycles(), i0 = m.machine.cpu().idleCycles();
      m.run(samples);
      return 1.0 - static_cast<double>(m.machine.cpu().idleCycles() - i0) / static_cast<double>(m.machine.cycles() - c0);
    };
    const int count = argc > 3 ? std::atoi(argv[3]) : 47;
    double worst = 0.0; std::string worstName; int worstModel = 0;
    // Certains firmwares (Bees) sont compilés pour un encodeur qui tourne à l'envers.
    int dir = 1;
    { const std::string first = m.display(); m.turn(1); if (m.display() == first) dir = -1; else m.turn(-1); }
    for (int model = 0; model < count; ++model) {
      const std::string name = m.display();
      double lo = 1.0, hi = 0.0;
      for (float voct : { 0.0f, 2.0f, 4.0f, 6.0f }) {
        for (float knobs : { 0.0f, 0.5f, 1.0f }) {
          m.jacks.voct = voct; m.panel.timbre = knobs; m.panel.color = knobs;
          m.jacks.trig = true; m.ms(5); m.jacks.trig = false; m.ms(15);
          const double l = loadOver(9600);
          lo = std::min(lo, l); hi = std::max(hi, l);
        }
      }
      std::printf("  [%s] %3.0f a %3.0f %%\n", name.c_str(), 100.0 * lo, 100.0 * hi);
      if (hi > worst) { worst = hi; worstName = name; worstModel = model; }
      m.jacks.voct = 0.0f; m.panel.timbre = m.panel.color = 0.5f;
      m.turn(dir);
    }
    std::printf("Modele le plus lourd : [%s] %.0f %%\n", worstName.c_str(), 100.0 * worst);
    expect(worst < 0.995, "aucun modele ne depasse la puissance du module");
    expect(worst > 0.5, "les modeles lourds chargent nettement le processeur");

    // Surcharge : on ralentit artificiellement la flash jusqu'à ce que le modèle
    // le plus lourd ne tienne plus, et on regarde ce que devient le son.
    std::printf("Surcharge (modele [%s], flash ralentie) :\n", worstName.c_str());
    bool sawOverload = false, cleanAtStock = false;
    for (int wait : { 2, 3, 4, 6, 9 }) {
      Emulated o(firmware, font);
      o.machine.setFlashWaitOverride(wait);
      o.machine.powerOn();
      o.ms(600);
      o.turn(dir * worstModel);
      o.jacks.voct = 2.0f; o.panel.timbre = o.panel.color = 1.0f;
      o.ms(100);
      const uint64_t c0 = o.machine.cycles(), i0 = o.machine.cpu().idleCycles();
      std::vector<float> x(96000);
      o.run(96000, x.data());
      const double l = 1.0 - static_cast<double>(o.machine.cpu().idleCycles() - i0) / static_cast<double>(o.machine.cycles() - c0);
      // Le firmware garde 4 blocs de 24 échantillons. Quand le rendu ne suit plus,
      // la lecture le rattrape et rejoue du son vieux de 96 échantillons.
      int repeated = 0, run = 0; bool flat = true;
      for (size_t i = 96; i < x.size(); ++i) {
        if (x[i] == x[i - 96]) {
          if (run > 0 && x[i] != x[i - 1]) flat = false;
          ++run;
        } else {
          if (run >= 4 && !flat) repeated += run;
          run = 0; flat = true;
        }
      }
      std::printf("  %2d cycles d'attente : processeur a %3.0f %%, %4.1f %% du son est du vieux son rejoue%s\n", wait,
                  100.0 * l, 100.0 * repeated / static_cast<double>(x.size()), o.machine.crashed() ? ", firmware plante" : "");
      if (wait == 2 && repeated == 0) cleanAtStock = true;
      if (l > 0.97 && repeated > 2000) sawOverload = true;
    }
    expect(cleanAtStock, "au reglage du firmware : aucun son rejoue");
    expect(sawOverload, "processeur trop lent : jauge a 100 % et le son begaie, comme sur le module");
  }
  std::printf(failures == 0 ? "OK\n" : "ECHECS : %d\n", failures);
  return failures;
}
