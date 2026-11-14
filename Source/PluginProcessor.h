// Tresses — processeur audio.
//
// Le plugin héberge un module Braids émulé (Source/emu) : une puce STM32F103
// virtuelle exécute l'image binaire du firmware. Le plugin ne fait que trois choses :
//   1. transformer la position des potards et les tensions des jacks en codes
//      ADC (étage analogique virtuel) ;
//   2. jouer le rôle d'une interface MIDI -> CV branchée sur les jacks ;
//   3. convertir la sortie du DAC (96 kHz) à la fréquence du DAW.

#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "AnalogFrontEnd.h"
#include "BraidsMachine.h"
#include "Resampler.h"

namespace ids {
// Potards du module
inline constexpr const char* fine = "fine";
inline constexpr const char* coarse = "coarse";
inline constexpr const char* fmAtt = "fmAtt";
inline constexpr const char* timbre = "timbre";
inline constexpr const char* modulation = "modulation";
inline constexpr const char* color = "color";
// Interface MIDI -> CV (câbles virtuels)
inline constexpr const char* trigSource = "trigSource";
inline constexpr const char* fmSource = "fmSource";
inline constexpr const char* timbreSource = "timbreSource";
inline constexpr const char* colorSource = "colorSource";
inline constexpr const char* fmCv = "fmCv";
inline constexpr const char* timbreCv = "timbreCv";
inline constexpr const char* colorCv = "colorCv";
inline constexpr const char* bendRange = "bendRange";
}  // namespace ids

// Sources possibles pour les jacks FM, TIMBRE et COLOR.
enum class CvSource : int { kNone = 0, kModWheel, kVelocity, kAftertouch, kAutomation };

class TressesAudioProcessor : public juce::AudioProcessor {
 public:
  TressesAudioProcessor();
  ~TressesAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return JucePlugin_Name; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  // --- Pour l'interface ----------------------------------------------------------
  juce::AudioProcessorValueTreeState apvts;

  // Encodeur : crans (+ = horaire) et bouton, appliqués au bloc audio suivant.
  void encoderTurn(int steps) { encoderSteps_.fetch_add(steps); }
  void encoderPress(bool pressed) { encoderPressed_.store(pressed); }

  // Afficheur : segments allumés de chaque caractère et luminosité (0..1).
  tresses::emu::BraidsMachine::Display display() const;

  // Occupation du processeur du module (0..1), et arrêt éventuel sur erreur.
  float moduleLoad() const { return moduleLoad_.load(); }
  bool moduleCrashed() const { return moduleCrashed_.load(); }

  // Activité des jacks, pour les LED de l'interface (0..1 ou -1..1).
  struct JackMeters {
    float trig = 0, voct = 0, fm = 0, timbre = 0, color = 0, out = 0;
  };
  JackMeters jackMeters() const;

 private:
  void powerOn(const uint8_t* settings, size_t settingsSize);
  void readPanel();
  void handleMidi(const juce::MidiMessage& message);
  void updateJacks();
  void renderRange(float* out, int start, int end);
  void pullModuleBlock(float* dst);
  float sourceVolts(CvSource source, float automationVolts) const;

  struct Raw {
    std::atomic<float>* fine = nullptr;
    std::atomic<float>* coarse = nullptr;
    std::atomic<float>* fmAtt = nullptr;
    std::atomic<float>* timbre = nullptr;
    std::atomic<float>* modulation = nullptr;
    std::atomic<float>* color = nullptr;
    std::atomic<float>* trigSource = nullptr;
    std::atomic<float>* fmSource = nullptr;
    std::atomic<float>* timbreSource = nullptr;
    std::atomic<float>* colorSource = nullptr;
    std::atomic<float>* fmCv = nullptr;
    std::atomic<float>* timbreCv = nullptr;
    std::atomic<float>* colorCv = nullptr;
    std::atomic<float>* bendRange = nullptr;
  } raw_;

  // Module émulé.
  tresses::emu::BraidsMachine machine_;
  uint32_t uid_[3] = { 0, 0, 0 };
  juce::SpinLock moduleLock_;

  tresses::PanelState panel_;
  tresses::JackState jacks_;
  tresses::Resampler resampler_;

  // Interface MIDI -> CV.
  std::array<int, 16> heldNotes_ {};
  int numHeld_ = 0;
  int currentNote_ = 36;     // 0 V au repos
  float velocity_ = 0.0f;
  float modWheel_ = 0.0f;
  float aftertouch_ = 0.0f;
  float bend_ = 0.0f;
  bool retrigger_ = false;

  std::atomic<int> encoderSteps_ { 0 };
  std::atomic<bool> encoderPressed_ { false };
  std::atomic<uint64_t> displayWord_ { 0 };
  std::atomic<float> displayBrightness_ { 0.0f };
  std::atomic<float> moduleLoad_ { 0.0f };
  std::atomic<bool> moduleCrashed_ { false };
  uint64_t loadCycles_ = 0, loadIdle_ = 0;
  std::array<std::atomic<float>, 6> meters_ {};
  float outPeak_ = 0.0f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TressesAudioProcessor)
};
