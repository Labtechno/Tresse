// Tresses — banc de test du module virtuel (sans JUCE).
//
//   module_test boot                 démarrage, afficheur, navigation au menu
//   module_test pitch                justesse après la calibration d'usine
//   module_test instances            plusieurs modules en parallèle
//   module_test render <s> <out.wav> rendu à 96 kHz d'une séquence de notes
//   module_test cpu                  coût de l'émulation sur cette machine

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "AnalogFrontEnd.h"
#include "FirmwareSlot.h"

using namespace tresses;

namespace {

struct Module {
  int index = -1;
  const FirmwareSlotApi* api = nullptr;
  PanelState panel;
  JackState jacks;

  explicit Module(bool calibrate = true) {
    index = claimFirmwareSlot();
    if (index < 0) { std::printf("plus de slot libre\n"); std::exit(1); }
    api = firmwareSlot(index);
    uint32_t uid[3] = { 0x12345678u, 0x9abcdef0u, 0x0badcafeu + static_cast<uint32_t>(index) };
    uint16_t codes[3];
    analog::factoryCalibrationCodes(codes);
    api->power_on(nullptr, 0, uid, analog::frame(panel, jacks), calibrate, codes);
  }
  ~Module() { releaseFirmwareSlot(index); }

  void run(int n, float* out = nullptr) {
    std::vector<FrameInput> in(static_cast<size_t>(n), analog::frame(panel, jacks));
    std::vector<float> tmp(static_cast<size_t>(n));
    api->run(in.data(), out ? out : tmp.data(), n);
  }
  void ms(int milliseconds) { run(96 * milliseconds); }

  std::string display() {
    DisplayState d;
    api->display(&d);
    std::string s;
    for (char c : d.text) {
      const unsigned char u = static_cast<unsigned char>(c);
      if (u >= 32 && u < 127) s += c; else { char b[8]; std::snprintf(b, 8, "\\x%02X", u); s += b; }
    }
    return s;
  }

  void turn(int steps) { api->encoder(steps, false); ms(10 + 5 * std::abs(steps)); }
  void click() { api->encoder(0, true); ms(30); api->encoder(0, false); ms(30); }
  void longPress() { api->encoder(0, true); ms(1000); api->encoder(0, false); ms(30); }
};

double measureF0(const std::vector<float>& x, double sr) {
  std::vector<double> t;
  for (size_t i = 1; i < x.size(); ++i)
    if (x[i - 1] < 0.0f && x[i] >= 0.0f) t.push_back((i - 1) + x[i - 1] / (x[i - 1] - x[i]));
  if (t.size() < 3) return 0.0;
  return (t.size() - 1) / ((t.back() - t.front()) / sr);
}

void writeWav(const std::string& path, const std::vector<float>& data, int rate) {
  FILE* f = std::fopen(path.c_str(), "wb");
  const uint32_t bytes = static_cast<uint32_t>(data.size() * 4);
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f); u32(16);
  u16(3); u16(1); u32(static_cast<uint32_t>(rate)); u32(static_cast<uint32_t>(rate) * 4); u16(4); u16(32);
  std::fwrite("data", 1, 4, f); u32(bytes); std::fwrite(data.data(), 4, data.size(), f);
  std::fclose(f);
}

int failures = 0;
void expect(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "OK" : "ECHEC", what);
  if (!ok) ++failures;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string cmd = argc > 1 ? argv[1] : "boot";

  if (cmd == "boot") {
    Module m;
    std::printf("Fréquence d'échantillonnage du firmware : %d Hz\n", m.api->sample_rate());
    std::printf("Démarrage : ");
    for (int i = 0; i < 10; ++i) { m.ms(50); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\n");
    expect(m.display() == "CSAW", "affiche CSAW après l'animation de démarrage");
    m.turn(1);
    std::printf("Un cran horaire : [%s]\n", m.display().c_str());
    expect(m.display() != "CSAW", "l'encodeur change de modèle");
    m.turn(-1);
    expect(m.display() == "CSAW", "retour à CSAW");
    m.click();
    std::printf("Clic : [%s]\n", m.display().c_str());
    expect(m.display() == "WAVE", "le menu s'ouvre sur WAVE");
    std::printf("Menu : ");
    for (int i = 0; i < 24; ++i) { m.turn(1); std::printf("[%s] ", m.display().c_str()); }
    std::printf("\n");
    m.turn(-24);
    m.turn(2);  // BITS
    expect(m.display() == "BITS", "WAVE > META > BITS");
    m.click();
    std::printf("Édition BITS : [%s]\n", m.display().c_str());
    m.turn(-2);
    std::printf("BITS -2 : [%s]\n", m.display().c_str());
    expect(m.display() == "8BIT", "BITS passe à 8BIT");
    m.click();
    m.turn(-2);
    m.click();  // entre dans WAVE -> sauvegarde
    const uint32_t v = m.api->flash_version();
    std::printf("Retour au modèle : [%s], version flash %u\n", m.display().c_str(), v);
    expect(m.display() == "CSAW", "retour à la sélection du modèle");

    // Redémarrage avec la flash sauvegardée : BITS doit être conservé.
    std::vector<uint8_t> flash(kFlashImageSize);
    m.api->read_flash(flash.data());
    std::vector<uint8_t> settings(m.api->settings_size());
    m.api->read_settings(settings.data());
    std::printf("Taille des réglages : %zu octets, BITS = %d\n", settings.size(), settings[1]);
    expect(settings[1] == 4, "réglage BITS = 4 (8BIT) en RAM");
    uint32_t uid[3] = { 1, 2, 3 };
    uint16_t codes[3];
    analog::factoryCalibrationCodes(codes);
    m.api->power_on(flash.data(), flash.size(), uid, analog::frame(m.panel, m.jacks), false, codes);
    m.ms(600);
    std::vector<uint8_t> after(m.api->settings_size());
    m.api->read_settings(after.data());
    expect(after[1] == 4, "BITS retrouvé après redémarrage (flash)");
    expect(std::memcmp(after.data(), settings.data(), settings.size()) == 0,
           "tous les réglages (calibration comprise) relus depuis la flash");
  } else if (cmd == "pitch") {
    Module m;
    m.ms(500);
    // Modèle FM (index 25 dans le firmware officiel) avec TIMBRE à 0 : sinus pur.
    m.turn(argc > 2 ? std::atoi(argv[2]) : 25);
    std::printf("Modèle : [%s]\n", m.display().c_str());
    m.panel.timbre = 0.0f;
    for (int note : { 36, 48, 60, 69, 81, 96 }) {
      m.jacks.voct = static_cast<float>(note - 36) / 12.0f;
      m.ms(100);
      std::vector<float> buf(96000 / 2);
      m.run(static_cast<int>(buf.size()), buf.data());
      const double f0 = measureF0(buf, 96000.0);
      const double expected = 440.0 * std::pow(2.0, (note - 69) / 12.0);
      const double cents = 1200.0 * std::log2(f0 / expected);
      std::printf("  note %3d : %9.3f Hz (attendu %9.3f) écart %+.2f cents\n", note, f0, expected, cents);
      if (std::abs(cents) > 3.0) ++failures;
    }
    // FINE au maximum : +1 demi-ton.
    m.jacks.voct = (69 - 36) / 12.0f;
    m.panel.fine = 1.0f;
    m.ms(800);  // le filtrage de FINE prend quelques dizaines de ms
    std::vector<float> buf(48000);
    m.run(48000, buf.data());
    std::printf("  FINE à fond : %.2f Hz (attendu ~466.16)\n", measureF0(buf, 96000.0));
    m.panel.fine = 0.0f;
    // COARSE à fond : +4 octaves (fenêtre ADC limitée à +6 V).
    m.jacks.voct = 0.0f;
    m.panel.coarse = 1.0f;
    m.ms(100);
    m.run(48000, buf.data());
    std::printf("  COARSE à fond, V/OCT 0 V : %.2f Hz (attendu ~1046.5)\n", measureF0(buf, 96000.0));
  } else if (cmd == "instances") {
    std::vector<std::unique_ptr<Module>> mods;
    for (int i = 0; i < 4; ++i) mods.push_back(std::make_unique<Module>());
    // Chacun dans son thread, comme les threads audio d'un DAW.
    std::vector<std::thread> threads;
    std::vector<double> f0(4);
    for (int i = 0; i < 4; ++i) {
      threads.emplace_back([&, i] {
        Module& m = *mods[static_cast<size_t>(i)];
        m.ms(500);
        m.turn(25);
        m.panel.timbre = 0.0f;
        m.jacks.voct = static_cast<float>(i);  // C1, C2, C3, C4
        m.ms(100);
        std::vector<float> buf(48000);
        m.run(48000, buf.data());
        f0[static_cast<size_t>(i)] = measureF0(buf, 96000.0);
      });
    }
    for (auto& t : threads) t.join();
    for (int i = 0; i < 4; ++i) std::printf("  module %d : %.2f Hz [%s]\n", i, f0[static_cast<size_t>(i)], mods[static_cast<size_t>(i)]->display().c_str());
    expect(std::abs(f0[1] / f0[0] - 2.0) < 0.01 && std::abs(f0[3] / f0[2] - 2.0) < 0.01,
           "4 modules indépendants, une octave d'écart chacun");
  } else if (cmd == "render" && argc == 4) {
    Module m;
    m.ms(500);
    m.turn(std::atoi(argv[2]));
    std::vector<float> out;
    const int notes[] = { 36, 43, 48, 46, 41, 39, 36, 31 };
    for (int k = 0; k < 16; ++k) {
      m.jacks.voct = (notes[k % 8] - 24) / 12.0f;
      m.jacks.trig = true;
      std::vector<float> buf(96000 / 4);
      m.run(static_cast<int>(buf.size()), buf.data());
      out.insert(out.end(), buf.begin(), buf.end());
      m.jacks.trig = false;
      m.run(static_cast<int>(buf.size()), buf.data());
      out.insert(out.end(), buf.begin(), buf.end());
    }
    writeWav(argv[3], out, 96000);
    std::printf("modèle [%s], %zu échantillons\n", m.display().c_str(), out.size());
    return 0;
  } else if (cmd == "cpu") {
    Module m;
    m.ms(500);
    for (int model : { 0, 25, 28, 37, 44, 46 }) {
      m.turn(model);
      m.ms(50);
      auto t0 = std::chrono::steady_clock::now();
      m.run(96000 * 5);
      auto t1 = std::chrono::steady_clock::now();
      std::printf("  [%s] %.2f %% d'un coeur\n", m.display().c_str(),
                  100.0 * std::chrono::duration<double>(t1 - t0).count() / 5.0);
      m.turn(-model);
    }
  }

  std::printf(failures == 0 ? "OK\n" : "ECHECS : %d\n", failures);
  return failures;
}
