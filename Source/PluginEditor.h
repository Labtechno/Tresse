// Tresses — interface : façade du module + interface MIDI -> CV.

#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace ui {

namespace colours {
// Façade : les couleurs du Braids d'origine (relevées sur une photo du module).
const juce::Colour panel { 0xffecebe9 };      // aluminium clair
const juce::Colour panelDark { 0xffe2e1de };
const juce::Colour topo { 0xffd2d1cd };       // lignes topographiques du fond
const juce::Colour ink { 0xff1b1b1d };        // sérigraphie
const juce::Colour inkDim { 0xff5f5e5a };
const juce::Colour capWhite { 0xfff0f1ed };   // capuchons FINE, COARSE, FM
const juce::Colour teal { 0xff00989d };       // capuchons TIMBRE, MODULATION
const juce::Colour tealLabel { 0xff2f7f8c };  // étiquettes TIMBRE (texte blanc lisible)
const juce::Colour magenta { 0xffa21e4d };    // COLOR
const juce::Colour slate { 0xff5d6371 };      // case OUT
// Autour du module.
const juce::Colour background { 0xff2b2d31 };
const juce::Colour interface { 0xff30343b };  // panneau MIDI -> CV
const juce::Colour field { 0xff22252a };
const juce::Colour moduleEdge { 0xff454a52 };
const juce::Colour text { 0xfff2f2f0 };
const juce::Colour textDim { 0xffaeb3bb };
const juce::Colour led { 0xffff5a3c };        // afficheur
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
  juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                            bool highlighted, bool down) override;
  juce::Label* createSliderTextBox(juce::Slider&) override;
};

// Potard à jupe colorée : nom puis valeur en dessous. `cap` est la couleur du
// capuchon ; `labelBox` (si non transparente) encadre le nom, comme TIMBRE et COLOR
// sur la façade ; `onDark` pour le panneau MIDI -> CV.
class Knob : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const char* paramId,
       const juce::String& name, juce::Colour cap, const juce::String& tooltip,
       juce::Colour labelBox = juce::Colours::transparentBlack, bool onDark = false);
  void resized() override;
  void paint(juce::Graphics&) override;

 private:
  juce::String name_;
  juce::Colour labelBox_;
  bool onDark_;
  juce::Rectangle<int> labelArea_, valueArea_;
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
  Jack(const juce::String& name, bool output,
       juce::Colour labelBox = juce::Colours::transparentBlack);
  void setPatched(bool patched) { if (patched != patched_) { patched_ = patched; repaint(); } }
  void setLevel(float level);
  void paint(juce::Graphics&) override;

 private:
  juce::String name_;
  bool output_;
  juce::Colour labelBox_;
  bool patched_ = false;
  float level_ = 0.0f;
};

// Sélecteur de source avec son nom.
class SourceSelect : public juce::Component {
 public:
  SourceSelect(juce::AudioProcessorValueTreeState& state, const char* paramId,
               const juce::String& jackName, const juce::StringArray& items,
               const juce::String& tooltip,
               juce::Colour labelBox = juce::Colours::transparentBlack);
  void resized() override;
  void paint(juce::Graphics&) override;

 private:
  juce::String jackName_;
  juce::Colour labelBox_;
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
  void showFirmwareMenu();
  void chooseFirmwareFile();
  void refreshFirmwareButton();

  TressesAudioProcessor& processor_;
  ui::LookAndFeel lookAndFeel_;
  juce::TooltipWindow tooltips_ { this, 600 };

  juce::Rectangle<int> moduleArea_, interfaceArea_;
  juce::Path topo_;   // lignes topographiques du fond de la façade
  int shownLoad_ = -1;

  ui::SegmentDisplay display_;
  juce::TextButton firmwareButton_;
  std::unique_ptr<juce::FileChooser> fileChooser_;
  ui::Encoder encoder_;
  std::unique_ptr<ui::Knob> fine_, coarse_, fm_, timbre_, modulation_, color_;
  std::unique_ptr<ui::Jack> jackTrig_, jackVoct_, jackFm_, jackTimbre_, jackColor_, jackOut_;

  std::unique_ptr<ui::SourceSelect> trigSource_, fmSource_, timbreSource_, colorSource_;
  std::unique_ptr<ui::Knob> fmCv_, timbreCv_, colorCv_, bendRange_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TressesAudioProcessorEditor)
};
