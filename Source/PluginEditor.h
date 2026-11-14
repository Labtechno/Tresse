// Tresses — interface : façade du module + interface MIDI -> CV.

#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace ui {

namespace colours {
const juce::Colour background { 0xff121416 };
const juce::Colour module { 0xff1f2226 };
const juce::Colour moduleEdge { 0xff32363c };
const juce::Colour interface { 0xff191b1e };
const juce::Colour text { 0xffe9e6e1 };
const juce::Colour textDim { 0xff8b9197 };
const juce::Colour knob { 0xffff8a3d };       // potards du module
const juce::Colour attenuverter { 0xffffc46b };
const juce::Colour cable { 0xff3fc1c9 };      // interface MIDI -> CV
const juce::Colour led { 0xffff3b2f };        // afficheur
}  // namespace colours

class LookAndFeel : public juce::LookAndFeel_V4 {
 public:
  LookAndFeel();
  void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                        float sliderPos, float startAngle, float endAngle,
                        juce::Slider&) override;
  void drawComboBox(juce::Graphics&, int width, int height, bool down, int bx, int by,
                    int bw, int bh, juce::ComboBox&) override;
  juce::Font getComboBoxFont(juce::ComboBox&) override;
  juce::Font getPopupMenuFont() override;
  juce::Label* createSliderTextBox(juce::Slider&) override;
};

// Potard : nom au-dessus, valeur en dessous.
class Knob : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const char* paramId,
       const juce::String& name, juce::Colour accent, const juce::String& tooltip);
  void resized() override;
  void paint(juce::Graphics&) override;

 private:
  juce::String name_;
  juce::Rectangle<int> labelArea_;
  juce::Slider slider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

// Afficheur 4 caractères 14 segments : les segments allumés viennent directement
// du registre à décalage que pilote le firmware.
class SegmentDisplay : public juce::Component {
 public:
  void setState(const tresses::emu::BraidsMachine::Display& state);
  void paint(juce::Graphics&) override;

 private:
  void paintDigit(juce::Graphics&, juce::Rectangle<float> cell, uint16_t segments,
                  float intensity) const;

  tresses::emu::BraidsMachine::Display state_ { { 0, 0, 0, 0 }, 0.0f };
};

// Encodeur cranté sans butée, avec bouton-poussoir.
class Encoder : public juce::Component,
                public juce::SettableTooltipClient,
                private juce::Timer {
 public:
  explicit Encoder(TressesAudioProcessor& processor);
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

 private:
  void timerCallback() override;
  void turn(int steps);

  TressesAudioProcessor& processor_;
  int position_ = 0;          // crans cumulés, pour le dessin
  bool down_ = false;
  bool turning_ = false;
  bool pressed_ = false;      // bouton enfoncé côté module
  float dragAccumulator_ = 0.0f;
  float wheelAccumulator_ = 0.0f;
  juce::Point<float> lastDrag_;
  double downTime_ = 0.0;
  double releaseAt_ = 0.0;
};

// Jack : nom, LED d'activité et câble quand quelque chose y est branché.
class Jack : public juce::Component {
 public:
  Jack(const juce::String& name, bool output);
  void setPatched(bool patched) { if (patched != patched_) { patched_ = patched; repaint(); } }
  void setLevel(float level);
  void paint(juce::Graphics&) override;

 private:
  juce::String name_;
  bool output_;
  bool patched_ = false;
  float level_ = 0.0f;
};

// Sélecteur de source avec son nom.
class SourceSelect : public juce::Component {
 public:
  SourceSelect(juce::AudioProcessorValueTreeState& state, const char* paramId,
               const juce::String& jackName, const juce::StringArray& items,
               const juce::String& tooltip);
  void resized() override;
  void paint(juce::Graphics&) override;

 private:
  juce::String jackName_;
  juce::ComboBox box_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment_;
};

}  // namespace ui

class TressesAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer {
 public:
  explicit TressesAudioProcessorEditor(TressesAudioProcessor&);
  ~TressesAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;

  TressesAudioProcessor& processor_;
  ui::LookAndFeel lookAndFeel_;
  juce::TooltipWindow tooltips_ { this, 600 };

  juce::Rectangle<int> moduleArea_, interfaceArea_;
  int shownLoad_ = -1;

  ui::SegmentDisplay display_;
  ui::Encoder encoder_;
  std::unique_ptr<ui::Knob> fine_, coarse_, fm_, timbre_, modulation_, color_;
  std::unique_ptr<ui::Jack> jackTrig_, jackVoct_, jackFm_, jackTimbre_, jackColor_, jackOut_;

  std::unique_ptr<ui::SourceSelect> trigSource_, fmSource_, timbreSource_, colorSource_;
  std::unique_ptr<ui::Knob> fmCv_, timbreCv_, colorCv_, bendRange_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TressesAudioProcessorEditor)
};
