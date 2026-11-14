// Tresses — processeur audio

#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

#include "PluginEditor.h"

namespace {

using APF = juce::AudioParameterFloat;
using API = juce::AudioParameterInt;
using APC = juce::AudioParameterChoice;

juce::ParameterID pid(const char* id) { return juce::ParameterID { id, 1 }; }

std::unique_ptr<APF> floatParam(const char* id, const juce::String& name, float lo, float hi,
                                float def, std::function<juce::String(float)> toText) {
  auto attributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
      [toText](float v, int) { return toText(v); });
  return std::make_unique<APF>(pid(id), name, juce::NormalisableRange<float> { lo, hi }, def,
                               attributes);
}

juce::String percent(float v) {
  const int p = juce::roundToInt(v * 100.0f);
  return juce::String(p) + " %";
}

juce::String signedPercent(float v) {
  const int p = juce::roundToInt(v * 100.0f);
  return (p > 0 ? "+" : "") + juce::String(p) + " %";
}

juce::String volts(float v) {
  return (v > 0.0049f ? "+" : "") + juce::String(v, 2) + " V";
}

const juce::StringArray kCvSources {
  juce::String::fromUTF8("Rien"),
  juce::String::fromUTF8("Molette"),
  juce::String::fromUTF8("Vélocité"),
  juce::String::fromUTF8("Aftertouch"),
  juce::String::fromUTF8("Automation")
};

constexpr uint64_t kDisplayValid = uint64_t { 1 } << 40;

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
TressesAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  // --- Les six potards du module -------------------------------------------------
  layout.add(floatParam(ids::fine, "Fine", -1.0f, 1.0f, 0.0f, [](float v) {
    const int c = juce::roundToInt(v * 100.0f);
    return (c > 0 ? "+" : "") + juce::String(c) + " ct";
  }));
  layout.add(floatParam(ids::coarse, "Coarse", -1.0f, 1.0f, 0.0f, [](float v) {
    const float st = v * 48.0f;
    return (st > 0.049f ? "+" : "") + juce::String(st, 1) + " st";
  }));
  layout.add(floatParam(ids::fmAtt, "FM", -1.0f, 1.0f, 0.0f, signedPercent));
  layout.add(floatParam(ids::timbre, "Timbre", 0.0f, 1.0f, 0.5f, percent));
  layout.add(floatParam(ids::modulation, "Modulation", -1.0f, 1.0f, 0.0f, signedPercent));
  layout.add(floatParam(ids::color, "Color", 0.0f, 1.0f, 0.5f, percent));

  // --- Interface MIDI -> CV : ce qui est branché dans les jacks ------------------------
  layout.add(std::make_unique<APC>(pid(ids::trigSource), "TRIG source",
                                   juce::StringArray { "Notes MIDI", "Rien" }, 0));
  layout.add(std::make_unique<APC>(pid(ids::fmSource), "FM source", kCvSources, 0));
  layout.add(std::make_unique<APC>(pid(ids::timbreSource), "TIMBRE source", kCvSources, 0));
  layout.add(std::make_unique<APC>(pid(ids::colorSource), "COLOR source", kCvSources, 0));
  layout.add(floatParam(ids::fmCv, "FM CV", -5.0f, 5.0f, 0.0f, volts));
  layout.add(floatParam(ids::timbreCv, "TIMBRE CV", 0.0f, 5.0f, 0.0f, volts));
  layout.add(floatParam(ids::colorCv, "COLOR CV", 0.0f, 5.0f, 0.0f, volts));
  layout.add(std::make_unique<API>(pid(ids::bendRange), "Bend range", 0, 12, 2,
                                   juce::AudioParameterIntAttributes().withStringFromValueFunction(
                                       [](int v, int) {
                                         return juce::String::fromUTF8("\xc2\xb1") +
                                                juce::String(v) + " st";
                                       })));
  return layout;
}

TressesAudioProcessor::TressesAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "TRESSES", createParameterLayout()) {
  auto get = [this](const char* id) {
    auto* p = apvts.getRawParameterValue(id);
    jassert(p != nullptr);
    return p;
  };
  raw_.fine = get(ids::fine);
  raw_.coarse = get(ids::coarse);
  raw_.fmAtt = get(ids::fmAtt);
  raw_.timbre = get(ids::timbre);
  raw_.modulation = get(ids::modulation);
  raw_.color = get(ids::color);
  raw_.trigSource = get(ids::trigSource);
  raw_.fmSource = get(ids::fmSource);
  raw_.timbreSource = get(ids::timbreSource);
  raw_.colorSource = get(ids::colorSource);
  raw_.fmCv = get(ids::fmCv);
  raw_.timbreCv = get(ids::timbreCv);
  raw_.colorCv = get(ids::colorCv);
  raw_.bendRange = get(ids::bendRange);

  for (auto& m : meters_) m.store(0.0f);

  slot_ = tresses::claimFirmwareSlot();
  module_ = tresses::firmwareSlot(slot_);

  // Chaque module a son propre identifiant de puce (il sert à la signature SIGN).
  auto& random = juce::Random::getSystemRandom();
  for (auto& word : uid_) word = static_cast<uint32_t>(random.nextInt());

  resampler_.prepare(tresses::kModuleSampleRate, 48000.0);
  powerOn(nullptr, 0, true);  // module neuf, calibré en usine
}

TressesAudioProcessor::~TressesAudioProcessor() {
  tresses::releaseFirmwareSlot(slot_);
}

void TressesAudioProcessor::powerOn(const uint8_t* flash, size_t flashSize,
                                    bool factoryCalibrate) {
  if (module_ == nullptr) return;
  uint16_t codes[3];
  tresses::analog::factoryCalibrationCodes(codes);
  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  readPanel();
  updateJacks();
  module_->power_on(flash, flashSize, uid_, tresses::analog::frame(panel_, jacks_),
                    factoryCalibrate, codes);
  resampler_.reset();
}

void TressesAudioProcessor::prepareToPlay(double sampleRate, int) {
  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  const int moduleRate = module_ ? module_->sample_rate() : tresses::kModuleSampleRate;
  resampler_.prepare(moduleRate, sampleRate);
  setLatencySamples(resampler_.latencyInOutputSamples());
}

bool TressesAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto out = layouts.getMainOutputChannelSet();
  if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
    return false;
  return layouts.getMainInputChannelSet().isDisabled();
}

void TressesAudioProcessor::readPanel() {
  panel_.fine = raw_.fine->load();
  panel_.coarse = raw_.coarse->load();
  panel_.fm = raw_.fmAtt->load();
  panel_.timbre = raw_.timbre->load();
  panel_.modulation = raw_.modulation->load();
  panel_.color = raw_.color->load();
}

float TressesAudioProcessor::sourceVolts(CvSource source, float automationVolts) const {
  switch (source) {
    case CvSource::kModWheel: return 5.0f * modWheel_;
    case CvSource::kVelocity: return 5.0f * velocity_;
    case CvSource::kAftertouch: return 5.0f * aftertouch_;
    case CvSource::kAutomation: return automationVolts;
    case CvSource::kNone:
    default: return 0.0f;
  }
}

void TressesAudioProcessor::updateJacks() {
  auto source = [](const std::atomic<float>* p) {
    return static_cast<CvSource>(juce::roundToInt(p->load()));
  };
  const bool trigFromNotes = juce::roundToInt(raw_.trigSource->load()) == 0;
  jacks_.trig = trigFromNotes && numHeld_ > 0;
  // Interface MIDI -> CV : 0 V = note 36 (C1 dans Reason), 1 V/octave.
  jacks_.voct = static_cast<float>(currentNote_ - 36) / 12.0f +
                bend_ * raw_.bendRange->load() / 12.0f;
  jacks_.fm = sourceVolts(source(raw_.fmSource), raw_.fmCv->load());
  jacks_.timbre = sourceVolts(source(raw_.timbreSource), raw_.timbreCv->load());
  jacks_.color = sourceVolts(source(raw_.colorSource), raw_.colorCv->load());
}

void TressesAudioProcessor::handleMidi(const juce::MidiMessage& m) {
  if (m.isNoteOn()) {
    const int note = m.getNoteNumber();
    const bool gateWasHigh = numHeld_ > 0;
    int j = 0;
    for (int i = 0; i < numHeld_; ++i) {
      if (heldNotes_[static_cast<size_t>(i)] != note) heldNotes_[static_cast<size_t>(j++)] = heldNotes_[static_cast<size_t>(i)];
    }
    numHeld_ = j;
    if (numHeld_ == static_cast<int>(heldNotes_.size())) {
      std::rotate(heldNotes_.begin(), heldNotes_.begin() + 1, heldNotes_.end());
      --numHeld_;
    }
    heldNotes_[static_cast<size_t>(numHeld_++)] = note;
    currentNote_ = note;
    velocity_ = m.getFloatVelocity();
    // Note liée : la porte redescend un échantillon pour que TRIG voie un front.
    if (gateWasHigh) retrigger_ = true;
  } else if (m.isNoteOff()) {
    const int note = m.getNoteNumber();
    int j = 0;
    for (int i = 0; i < numHeld_; ++i) {
      if (heldNotes_[static_cast<size_t>(i)] != note) heldNotes_[static_cast<size_t>(j++)] = heldNotes_[static_cast<size_t>(i)];
    }
    numHeld_ = j;
    if (numHeld_ > 0) currentNote_ = heldNotes_[static_cast<size_t>(numHeld_ - 1)];
  } else if (m.isPitchWheel()) {
    bend_ = static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f;
  } else if (m.isControllerOfType(1)) {
    modWheel_ = static_cast<float>(m.getControllerValue()) / 127.0f;
  } else if (m.isChannelPressure()) {
    aftertouch_ = static_cast<float>(m.getChannelPressureValue()) / 127.0f;
  } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
    numHeld_ = 0;
  }
}

void TressesAudioProcessor::pullModuleBlock(float* dst) {
  // Relu tous les 24 échantillons à 96 kHz : l'automation est aussi fluide que
  // la lecture de l'ADC du module (un balayage complet tous les 12 échantillons).
  readPanel();
  updateJacks();
  tresses::FrameInput frames[tresses::Resampler::kBlock];
  const tresses::FrameInput f = tresses::analog::frame(panel_, jacks_);
  std::fill(std::begin(frames), std::end(frames), f);
  if (retrigger_) {
    frames[0].trig = 0;
    retrigger_ = false;
  }
  module_->run(frames, dst, tresses::Resampler::kBlock);
}

void TressesAudioProcessor::renderRange(float* out, int start, int end) {
  for (int n = start; n < end; ++n) {
    out[n] = resampler_.process([this](float* dst) { pullModuleBlock(dst); });
  }
}

void TressesAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int numSamples = buffer.getNumSamples();
  buffer.clear();
  if (module_ == nullptr || buffer.getNumChannels() == 0 || numSamples == 0) return;

  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  module_->encoder(encoderSteps_.exchange(0), encoderPressed_.load());

  float* out = buffer.getWritePointer(0);
  int position = 0;
  for (const auto metadata : midi) {
    const int eventPosition = juce::jlimit(0, numSamples, metadata.samplePosition);
    renderRange(out, position, eventPosition);
    position = eventPosition;
    handleMidi(metadata.getMessage());
  }
  renderRange(out, position, numSamples);

  if (buffer.getNumChannels() > 1) buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

  // Afficheur et LED pour l'interface.
  tresses::DisplayState d;
  module_->display(&d);
  uint64_t word = kDisplayValid | (static_cast<uint64_t>(d.brightness) << 32);
  for (int i = 0; i < 4; ++i) {
    word |= static_cast<uint64_t>(static_cast<uint8_t>(d.text[i])) << (8 * i);
  }
  displayWord_.store(word);

  const float peak = buffer.getMagnitude(0, 0, numSamples);
  outPeak_ = std::max(peak, outPeak_ * 0.8f);
  meters_[0].store(jacks_.trig ? 1.0f : 0.0f);
  meters_[1].store(jacks_.voct);
  meters_[2].store(jacks_.fm);
  meters_[3].store(jacks_.timbre);
  meters_[4].store(jacks_.color);
  meters_[5].store(outPeak_);
}

tresses::DisplayState TressesAudioProcessor::display() const {
  tresses::DisplayState d {};
  const uint64_t word = displayWord_.load();
  if ((word & kDisplayValid) == 0) {
    for (auto& c : d.text) c = ' ';
    d.brightness = 3;
    return d;
  }
  for (int i = 0; i < 4; ++i) d.text[i] = static_cast<char>((word >> (8 * i)) & 0xff);
  d.brightness = static_cast<uint8_t>((word >> 32) & 0xff);
  return d;
}

TressesAudioProcessor::JackMeters TressesAudioProcessor::jackMeters() const {
  JackMeters m;
  m.trig = meters_[0].load();
  m.voct = meters_[1].load();
  m.fm = meters_[2].load();
  m.timbre = meters_[3].load();
  m.color = meters_[4].load();
  m.out = meters_[5].load();
  return m;
}

juce::AudioProcessorEditor* TressesAudioProcessor::createEditor() {
  return new TressesAudioProcessorEditor(*this);
}

// --- État : paramètres + mémoire du module ---------------------------------------------
//
// Comme le vrai module, Tresses garde ses réglages dans sa flash : l'image de la
// flash est enregistrée dans le projet. Les réglages en RAM le sont aussi, pour
// retrouver exactement le son entendu même si le menu n'a pas été refermé (sur
// le module, la sauvegarde se fait en revenant à WAVE).

void TressesAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  auto state = apvts.copyState();
  if (module_ != nullptr) {
    juce::MemoryBlock flash(tresses::kFlashImageSize);
    juce::MemoryBlock settings;
    {
      const juce::SpinLock::ScopedLockType lock(moduleLock_);
      module_->read_flash(static_cast<uint8_t*>(flash.getData()));
      settings.setSize(module_->settings_size());
      module_->read_settings(static_cast<uint8_t*>(settings.getData()));
    }
    juce::ValueTree moduleTree("MODULE");
    moduleTree.setProperty("uid", juce::String::toHexString(static_cast<juce::int64>(uid_[0])) + ":" +
                                      juce::String::toHexString(static_cast<juce::int64>(uid_[1])) + ":" +
                                      juce::String::toHexString(static_cast<juce::int64>(uid_[2])),
                           nullptr);
    moduleTree.setProperty("flash", flash.toBase64Encoding(), nullptr);
    moduleTree.setProperty("ram", settings.toBase64Encoding(), nullptr);
    state.appendChild(moduleTree, nullptr);
  }
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void TressesAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  auto xml = getXmlFromBinary(data, sizeInBytes);
  if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;

  auto tree = juce::ValueTree::fromXml(*xml);
  auto moduleTree = tree.getChildWithName("MODULE");
  if (moduleTree.isValid()) tree.removeChild(moduleTree, nullptr);
  apvts.replaceState(tree);

  if (!moduleTree.isValid() || module_ == nullptr) return;

  const auto uidParts = juce::StringArray::fromTokens(moduleTree["uid"].toString(), ":", "");
  if (uidParts.size() == 3) {
    for (int i = 0; i < 3; ++i) {
      uid_[i] = static_cast<uint32_t>(uidParts[i].getHexValue64());
    }
  }
  juce::MemoryBlock flash, settings;
  const bool hasFlash = flash.fromBase64Encoding(moduleTree["flash"].toString()) &&
                        flash.getSize() > 0;
  settings.fromBase64Encoding(moduleTree["ram"].toString());

  powerOn(hasFlash ? static_cast<const uint8_t*>(flash.getData()) : nullptr,
          hasFlash ? flash.getSize() : 0, !hasFlash);

  if (settings.getSize() == module_->settings_size()) {
    const juce::SpinLock::ScopedLockType lock(moduleLock_);
    module_->write_settings(static_cast<const uint8_t*>(settings.getData()));
  }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new TressesAudioProcessor();
}
