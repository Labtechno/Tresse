// Tresses — test du processeur du plugin : chargement de firmware, réglages par
// firmware, enregistrement et relecture de l'état d'un projet.
//
//   plugin_test <autre_firmware.wav|.bin> [fichier_invalide]

#include <cmath>
#include <cstdio>

#include "../Source/PluginProcessor.h"

namespace {

int failures = 0;
void expect(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "OK" : "ECHEC", what);
  if (!ok) ++failures;
}

// Joue une note et renvoie sa fréquence (passages par zéro), après `settle` secondes.
double playNote(TressesAudioProcessor& p, int note, double seconds = 1.0) {
  const double sr = 48000.0;
  juce::AudioBuffer<float> buffer(2, 256);
  std::vector<float> rec;
  const int total = static_cast<int>(sr * seconds);
  for (int pos = 0; pos < total; pos += 256) {
    juce::MidiBuffer midi;
    if (pos == 0) midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    buffer.clear();
    p.processBlock(buffer, midi);
    if (pos > total / 2) for (int i = 0; i < 256; ++i) rec.push_back(buffer.getSample(0, i));
  }
  std::vector<double> t;
  for (size_t i = 1; i < rec.size(); ++i)
    if (rec[i - 1] < 0.0f && rec[i] >= 0.0f) t.push_back((i - 1) + rec[i - 1] / (rec[i - 1] - rec[i]));
  return t.size() < 3 ? 0.0 : (t.size() - 1) / ((t.back() - t.front()) / sr);
}

// Tourne l'encodeur / clique, en laissant le temps au module de réagir.
void idle(TressesAudioProcessor& p, double seconds) {
  juce::AudioBuffer<float> buffer(2, 256);
  juce::MidiBuffer midi;
  for (int pos = 0; pos < static_cast<int>(48000 * seconds); pos += 256) { buffer.clear(); p.processBlock(buffer, midi); }
}
void turn(TressesAudioProcessor& p, int steps) { p.encoderTurn(steps); idle(p, 0.05 + 0.01 * std::abs(steps)); }
void click(TressesAudioProcessor& p) { p.encoderPress(true); idle(p, 0.05); p.encoderPress(false); idle(p, 0.05); }

uint64_t shown(TressesAudioProcessor& p) {
  const auto d = p.display();
  uint64_t w = 0;
  for (int i = 0; i < 4; ++i) w |= static_cast<uint64_t>(d.segments[i]) << (16 * i);
  return w;
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI init;
  if (argc < 2) { std::printf("usage: plugin_test firmware.wav [invalide]\n"); return 2; }

  TressesAudioProcessor p;
  p.setPlayConfigDetails(0, 2, 48000.0, 256);
  p.prepareToPlay(48000.0, 256);
  idle(p, 0.8);
  std::printf("Firmware au demarrage : %s\n", p.firmwareName().toRawUTF8());
  const uint64_t csaw = shown(p);
  const double a4 = playNote(p, 69);
  std::printf("La (note 69) : %.2f Hz\n", a4);
  expect(std::abs(a4 - 440.0) < 0.5, "firmware fourni : juste avec ses reglages d'usine");

  // Un réglage propre au firmware fourni : modèle suivant, sauvé en flash.
  turn(p, 3); click(p); click(p); idle(p, 0.2);
  const uint64_t model3 = shown(p);
  expect(model3 != csaw, "firmware fourni : modele change et sauve");

  if (argc > 2) {
    const juce::String error = p.loadFirmwareFile(juce::File(argv[2]));
    std::printf("Fichier invalide : %s\n", error.toRawUTF8());
    expect(error.isNotEmpty() && p.firmwareIsBuiltIn(), "un fichier invalide est refuse sans rien changer");
  }

  const juce::String error = p.loadFirmwareFile(juce::File(argv[1]));
  idle(p, 0.8);
  std::printf("Apres chargement : %s %s %s\n", p.firmwareName().toRawUTF8(), error.toRawUTF8(), p.firmwareNote().toRawUTF8());
  expect(error.isEmpty() && !p.firmwareIsBuiltIn(), "le fichier est charge");
  expect(shown(p) == csaw, "le nouveau firmware demarre sur CSAW (ses propres reglages)");
  const double other = playNote(p, 69);
  std::printf("La avec ce firmware : %.2f Hz\n", other);
  expect(other > 400.0 && other < 480.0 && !p.moduleCrashed(), "le nouveau firmware joue, calibre automatiquement");
  turn(p, 5); click(p); click(p); idle(p, 0.2);   // un réglage propre à ce firmware
  const uint64_t otherModel = shown(p);

  // Enregistrement du projet, puis réouverture dans une nouvelle instance.
  juce::MemoryBlock state;
  p.getStateInformation(state);
  std::printf("Etat du projet : %zu octets\n", state.getSize());
  {
    TressesAudioProcessor q;
    q.setPlayConfigDetails(0, 2, 48000.0, 256);
    q.prepareToPlay(48000.0, 256);
    q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    idle(q, 0.8);
    expect(q.firmwareName() == p.firmwareName() && !q.firmwareIsBuiltIn(), "projet rouvert : meme firmware, embarque dans le projet");
    expect(shown(q) == otherModel, "projet rouvert : reglages du firmware retrouves");
    turn(q, -5); idle(q, 0.2);   // retour sur CSAW, où la hauteur se mesure bien
    const double f = playNote(q, 69);
    std::printf("La avant / apres reouverture : %.2f / %.2f Hz\n", other, f);
    expect(std::abs(f - other) < 0.5, "projet rouvert : meme hauteur (calibration retrouvee)");
    q.useBuiltInFirmware();
    idle(q, 0.8);
    expect(q.firmwareIsBuiltIn() && shown(q) == model3, "retour au firmware fourni : ses reglages a lui sont retrouves");
  }

  p.useBuiltInFirmware();
  idle(p, 0.8);
  expect(shown(p) == model3, "retour au firmware fourni dans la meme instance");
  p.loadFirmwareFile(juce::File(argv[1]));
  idle(p, 0.8);
  expect(shown(p) == otherModel, "rechargement du fichier : ses reglages sont retrouves");
  std::printf(failures == 0 ? "OK\n" : "ECHECS : %d\n", failures);
  return failures;
}
