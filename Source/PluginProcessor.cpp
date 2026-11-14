// Tresses — processeur audio

#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

#include "BinaryData.h"
#include "FactorySetup.h"
#include "FirmwareFile.h"
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

  // Chaque module a son propre identifiant de puce (il sert à la signature SIGN).
  auto& random = juce::Random::getSystemRandom();
  for (auto& word : uid_) word = static_cast<uint32_t>(random.nextInt());

  resampler_.prepare(96000.0, 48000.0);
  firmware_ = builtInFirmware();
  juce::String note;
  boot(factorySettings(firmware_, note));  // module neuf, réglages d'usine
}

// --- Firmware ------------------------------------------------------------------------------

TressesAudioProcessor::Firmware TressesAudioProcessor::builtInFirmware() {
  Firmware f;
  f.name = "Braids v1.9";
  f.image.assign(reinterpret_cast<const uint8_t*>(BinaryData::braids1_9_bin),
                 reinterpret_cast<const uint8_t*>(BinaryData::braids1_9_bin) + BinaryData::braids1_9_binSize);
  f.id = tresses::emu::firmwareId(f.image);
  f.builtIn = true;
  return f;
}

// La zone des réglages : de la fin de l'image (arrondie à une page de 1 Ko) à la
// fin de la flash. C'est là que le firmware range ses réglages et sa calibration.
size_t TressesAudioProcessor::settingsOffset(const Firmware& firmware) {
  return 0x4000 + ((firmware.image.size() + 1023) & ~size_t { 1023 });
}

// Réglages d'usine d'un firmware. Pour le firmware intégré, ils sont fournis avec le
// plugin. Pour un fichier chargé, un second module émulé exécute la procédure CAL du
// firmware (1 V puis 3 V), comme en sortie d'usine.
std::vector<uint8_t> TressesAudioProcessor::factorySettings(const Firmware& firmware,
                                                            juce::String& note) const {
  using Machine = tresses::emu::BraidsMachine;
  const size_t size = Machine::kFlashSize - settingsOffset(firmware);
  if (firmware.builtIn && static_cast<size_t>(BinaryData::braids1_9_factory_binSize) == size) {
    const auto* p = reinterpret_cast<const uint8_t*>(BinaryData::braids1_9_factory_bin);
    return std::vector<uint8_t>(p, p + size);
  }
  auto bench = std::make_unique<Machine>();
  bench->loadApplication(firmware.image.data(), firmware.image.size());
  bench->setUniqueId(uid_);
  bench->powerOn();
  uint16_t codes[3];
  tresses::analog::factoryCalibrationCodes(codes);
  const tresses::FrameInput idle = tresses::analog::frame(tresses::PanelState(), tresses::JackState());
  Machine::Inputs in;
  for (int i = 0; i < 4; ++i) in.adc[i] = idle.adc[i];
  in.fine = idle.fine;
  switch (tresses::emu::factorySetup(*bench, in, codes[0], codes[1])) {
    case tresses::emu::FactoryResult::kCalibrated: break;
    case tresses::emu::FactoryResult::kNoCalMenu:
      note = juce::String::fromUTF8("Ce firmware n'a pas de menu CAL. reconnu : il démarre sans calibration, "
                                    "les notes peuvent être fausses.");
      break;
    case tresses::emu::FactoryResult::kNotRunning:
      note = juce::String::fromUTF8("Ce firmware ne démarre pas sur le module émulé (rien à l'afficheur).");
      break;
  }
  const uint8_t* flash = bench->flashImage();
  return std::vector<uint8_t>(flash + settingsOffset(firmware), flash + Machine::kFlashSize);
}

std::vector<uint8_t> TressesAudioProcessor::currentSettings() {
  using Machine = tresses::emu::BraidsMachine;
  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  const uint8_t* flash = machine_.flashImage();
  return std::vector<uint8_t>(flash + settingsOffset(firmware_), flash + Machine::kFlashSize);
}

// Mise sous tension avec le firmware courant et la zone de réglages donnée.
void TressesAudioProcessor::boot(const std::vector<uint8_t>& settings) {
  using Machine = tresses::emu::BraidsMachine;
  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  std::memset(machine_.cpu().flash(), 0xff, Machine::kFlashSize);
  machine_.loadApplication(firmware_.image.data(), firmware_.image.size());
  const size_t offset = settingsOffset(firmware_);
  if (settings.size() == Machine::kFlashSize - offset) {
    std::memcpy(machine_.cpu().flash() + offset, settings.data(), settings.size());
  }
  machine_.setUniqueId(uid_);
  machine_.powerOn();
  loadCycles_ = machine_.cycles();
  loadIdle_ = machine_.cpu().idleCycles();
  resampler_.reset();
}

// Change de firmware, comme un reflashage : les réglages du firmware quitté sont mis
// de côté, ceux du nouveau sont retrouvés (ou créés), puis le module redémarre.
void TressesAudioProcessor::switchFirmware(Firmware firmware) {
  savedSettings_[firmware_.id] = currentSettings();
  firmwareNote_ = {};
  std::vector<uint8_t> settings;
  const auto saved = savedSettings_.find(firmware.id);
  if (saved != savedSettings_.end() &&
      saved->second.size() == tresses::emu::BraidsMachine::kFlashSize - settingsOffset(firmware)) {
    settings = saved->second;
  } else {
    settings = factorySettings(firmware, firmwareNote_);
  }
  {
    const juce::SpinLock::ScopedLockType lock(moduleLock_);
    firmware_ = std::move(firmware);
  }
  boot(settings);
}

juce::String TressesAudioProcessor::loadFirmwareFile(const juce::File& file) {
  juce::MemoryBlock bytes;
  if (!file.loadFileAsData(bytes) || bytes.getSize() == 0) {
    return juce::String::fromUTF8("Impossible de lire ce fichier.");
  }
  tresses::emu::FirmwareImage image;
  std::string error;
  if (!tresses::emu::readFirmwareFile(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), image, error)) {
    return juce::String::fromUTF8(error.c_str());
  }
  Firmware f;
  f.name = file.getFileNameWithoutExtension();
  f.image = std::move(image.data);
  f.id = tresses::emu::firmwareId(f.image);
  f.builtIn = false;
  switchFirmware(std::move(f));
  return {};
}

void TressesAudioProcessor::useBuiltInFirmware() {
  if (!firmware_.builtIn) switchFirmware(builtInFirmware());
}

void TressesAudioProcessor::prepareToPlay(double sampleRate, int) {
  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  hostSampleRate_ = sampleRate;
  moduleSampleRate_ = machine_.sampleRate();
  resampler_.prepare(moduleSampleRate_, hostSampleRate_);
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
  using Inputs = tresses::emu::BraidsMachine::Inputs;
  const tresses::FrameInput f = tresses::analog::frame(panel_, jacks_);
  Inputs in;
  for (int i = 0; i < 4; ++i) in.adc[i] = f.adc[i];
  in.fine = f.fine;
  in.trig = f.trig != 0;
  Inputs frames[tresses::Resampler::kBlock];
  std::fill(std::begin(frames), std::end(frames), in);
  if (retrigger_) {
    frames[0].trig = false;
    retrigger_ = false;
  }
  machine_.run(frames, dst, tresses::Resampler::kBlock);
}

// Un firmware peut régler son timer audio sur une autre fréquence que 96 kHz.
void TressesAudioProcessor::followModuleSampleRate() {
  const double rate = machine_.sampleRate();
  if (std::abs(rate - moduleSampleRate_) > 0.5 && rate >= 4000.0 && rate <= 400000.0) {
    moduleSampleRate_ = rate;
    resampler_.prepare(moduleSampleRate_, hostSampleRate_);
  }
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
  if (buffer.getNumChannels() == 0 || numSamples == 0) return;

  const juce::SpinLock::ScopedLockType lock(moduleLock_);
  machine_.encoderTurn(encoderSteps_.exchange(0));
  machine_.encoderPress(encoderPressed_.load());
  followModuleSampleRate();

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

  // Afficheur, charge du processeur et LED pour l'interface.
  const auto d = machine_.display();
  uint64_t word = 0;
  for (int i = 0; i < 4; ++i) word |= static_cast<uint64_t>(d.segments[i]) << (16 * i);
  displayWord_.store(word);
  displayBrightness_.store(d.brightness);
  moduleCrashed_.store(machine_.crashed());
  const uint64_t cycles = machine_.cycles();
  if (cycles - loadCycles_ > tresses::emu::BraidsMachine::kCpuHz / 4) {
    const uint64_t idle = machine_.cpu().idleCycles();
    moduleLoad_.store(1.0f - static_cast<float>(idle - loadIdle_) / static_cast<float>(cycles - loadCycles_));
    loadCycles_ = cycles;
    loadIdle_ = idle;
  }

  const float peak = buffer.getMagnitude(0, 0, numSamples);
  outPeak_ = std::max(peak, outPeak_ * 0.8f);
  meters_[0].store(jacks_.trig ? 1.0f : 0.0f);
  meters_[1].store(jacks_.voct);
  meters_[2].store(jacks_.fm);
  meters_[3].store(jacks_.timbre);
  meters_[4].store(jacks_.color);
  meters_[5].store(outPeak_);
}

tresses::emu::BraidsMachine::Display TressesAudioProcessor::display() const {
  tresses::emu::BraidsMachine::Display d;
  const uint64_t word = displayWord_.load();
  for (int i = 0; i < 4; ++i) d.segments[i] = static_cast<uint16_t>(word >> (16 * i));
  d.brightness = displayBrightness_.load();
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
// Comme le vrai module, Tresses garde ses réglages dans sa flash : la zone des
// réglages est enregistrée dans le projet. À la réouverture, le module est remis
// sous tension avec cette flash, exactement comme un module qu'on rallume : les
// réglages du menu sont ceux de la dernière sauvegarde (retour sur WAVE).

void TressesAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  auto state = apvts.copyState();
  auto base64 = [](const std::vector<uint8_t>& bytes) {
    return juce::MemoryBlock(bytes.data(), bytes.size()).toBase64Encoding();
  };
  juce::ValueTree moduleTree("MODULE");
  moduleTree.setProperty("uid", juce::String::toHexString(static_cast<juce::int64>(uid_[0])) + ":" +
                                    juce::String::toHexString(static_cast<juce::int64>(uid_[1])) + ":" +
                                    juce::String::toHexString(static_cast<juce::int64>(uid_[2])),
                         nullptr);
  moduleTree.setProperty("firmwareName", firmware_.name, nullptr);
  moduleTree.setProperty("firmwareId", juce::String(firmware_.id), nullptr);
  // Le firmware chargé depuis un fichier voyage avec le projet.
  if (!firmware_.builtIn) moduleTree.setProperty("firmwareImage", base64(firmware_.image), nullptr);
  moduleTree.setProperty("settings", base64(currentSettings()), nullptr);
  for (const auto& entry : savedSettings_) {
    if (entry.first == firmware_.id) continue;
    juce::ValueTree other("OTHER_FIRMWARE_SETTINGS");
    other.setProperty("firmwareId", juce::String(entry.first), nullptr);
    other.setProperty("settings", base64(entry.second), nullptr);
    moduleTree.appendChild(other, nullptr);
  }
  state.appendChild(moduleTree, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void TressesAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  auto xml = getXmlFromBinary(data, sizeInBytes);
  if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;

  auto tree = juce::ValueTree::fromXml(*xml);
  auto moduleTree = tree.getChildWithName("MODULE");
  if (moduleTree.isValid()) tree.removeChild(moduleTree, nullptr);
  apvts.replaceState(tree);
  if (!moduleTree.isValid()) return;

  auto bytes = [](const juce::var& v) {
    juce::MemoryBlock block;
    block.fromBase64Encoding(v.toString());
    const auto* p = static_cast<const uint8_t*>(block.getData());
    return std::vector<uint8_t>(p, p + block.getSize());
  };
  const auto uidParts = juce::StringArray::fromTokens(moduleTree["uid"].toString(), ":", "");
  if (uidParts.size() == 3) {
    for (int i = 0; i < 3; ++i) uid_[i] = static_cast<uint32_t>(uidParts[i].getHexValue64());
  }

  Firmware firmware = builtInFirmware();
  if (moduleTree.hasProperty("firmwareImage")) {
    tresses::emu::FirmwareImage image;
    std::string error;
    const auto raw = bytes(moduleTree["firmwareImage"]);
    if (tresses::emu::readFirmwareFile(raw.data(), raw.size(), image, error)) {
      firmware.name = moduleTree["firmwareName"].toString();
      firmware.image = std::move(image.data);
      firmware.id = tresses::emu::firmwareId(firmware.image);
      firmware.builtIn = false;
    }
  }
  savedSettings_.clear();
  for (const auto& child : moduleTree) {
    if (child.hasType("OTHER_FIRMWARE_SETTINGS")) {
      savedSettings_[child["firmwareId"].toString().toStdString()] = bytes(child["settings"]);
    }
  }
  std::vector<uint8_t> settings = bytes(moduleTree["settings"]);
  firmwareNote_ = {};
  if (settings.size() != tresses::emu::BraidsMachine::kFlashSize - settingsOffset(firmware)) {
    settings = factorySettings(firmware, firmwareNote_);
  }
  {
    const juce::SpinLock::ScopedLockType lock(moduleLock_);
    firmware_ = std::move(firmware);
  }
  boot(settings);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new TressesAudioProcessor();
}
