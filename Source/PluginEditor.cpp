// Tresses — interface : façade du module + interface MIDI -> CV.

#include "PluginEditor.h"

#include <cmath>
#include <cstring>

#include "TressesConfig.h"

namespace ui {

namespace {

juce::Font uiFont(float height, bool bold = false) {
  return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour accentOf(const juce::Component& c, juce::Colour fallback) {
  const auto v = c.getProperties()["accent"];
  return v.isVoid() ? fallback : juce::Colour(static_cast<juce::uint32>(static_cast<juce::int64>(v)));
}

void setAccent(juce::Component& c, juce::Colour accent) {
  c.getProperties().set("accent", static_cast<juce::int64>(accent.getARGB()));
}

}  // namespace

// --- LookAndFeel -------------------------------------------------------------------

LookAndFeel::LookAndFeel() {
  setColour(juce::ResizableWindow::backgroundColourId, colours::background);
  setColour(juce::Slider::textBoxTextColourId, colours::text);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
  setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111315));
  setColour(juce::ComboBox::textColourId, colours::text);
  setColour(juce::ComboBox::outlineColourId, colours::moduleEdge);
  setColour(juce::ComboBox::arrowColourId, colours::textDim);
  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1b1d21));
  setColour(juce::PopupMenu::textColourId, colours::text);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::cable.withAlpha(0.35f));
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
  setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff0f1012));
  setColour(juce::TooltipWindow::textColourId, colours::text);
  setColour(juce::TooltipWindow::outlineColourId, colours::moduleEdge);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPos, float startAngle, float endAngle,
                                   juce::Slider& slider) {
  const auto accent = accentOf(slider, colours::knob);
  const auto area = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
  const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
  const auto centre = area.getCentre();
  const float lineWidth = juce::jmax(2.5f, radius * 0.11f);
  const float arcRadius = radius - lineWidth * 0.5f;
  const float angle = startAngle + sliderPos * (endAngle - startAngle);

  juce::Path track;
  track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
  g.setColour(juce::Colour(0xff34383d));
  g.strokePath(track, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

  const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
  const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
  if (std::abs(angle - from) > 0.001f) {
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        juce::jmin(from, angle), juce::jmax(from, angle), true);
    g.setColour(accent);
    g.strokePath(value, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
  }

  const float bodyRadius = radius * 0.7f;
  const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.fillEllipse(body.translated(0.0f, 2.5f));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a5057), body.getX(), body.getY(),
                                         juce::Colour(0xff1d2024), body.getX(), body.getBottom(),
                                         false));
  g.fillEllipse(body);
  g.setColour(juce::Colour(0xff565c63));
  g.drawEllipse(body.reduced(0.5f), 1.0f);

  const float inner = bodyRadius * 0.2f;
  const float outer = bodyRadius * 0.92f;
  const juce::Point<float> a(centre.x + inner * std::sin(angle), centre.y - inner * std::cos(angle));
  const juce::Point<float> b(centre.x + outer * std::sin(angle), centre.y - outer * std::cos(angle));
  g.setColour(colours::text);
  g.drawLine({ a, b }, juce::jmax(2.0f, radius * 0.075f));
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int,
                               int, int, juce::ComboBox& box) {
  auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                       static_cast<float>(height)).reduced(0.5f);
  g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
  g.fillRoundedRectangle(bounds, 5.0f);
  g.setColour(box.hasKeyboardFocus(true) ? colours::textDim : colours::moduleEdge);
  g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
  juce::Path arrow;
  const float cx = static_cast<float>(width) - 13.0f;
  const float cy = static_cast<float>(height) * 0.5f;
  arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
  g.setColour(colours::textDim);
  g.fillPath(arrow);
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.0f); }
juce::Font LookAndFeel::getPopupMenuFont() { return uiFont(14.0f); }

juce::Label* LookAndFeel::createSliderTextBox(juce::Slider& slider) {
  auto* label = LookAndFeel_V4::createSliderTextBox(slider);
  label->setFont(uiFont(12.0f));
  label->setColour(juce::Label::textColourId, colours::textDim);
  label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
  label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
  return label;
}

// --- Knob ------------------------------------------------------------------------------

Knob::Knob(juce::AudioProcessorValueTreeState& state, const char* paramId,
           const juce::String& name, juce::Colour accent, const juce::String& tooltip)
    : name_(name) {
  slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 90, 16);
  slider_.setTooltip(tooltip);
  setAccent(slider_, accent);
  addAndMakeVisible(slider_);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      state, paramId, slider_);
  if (auto* param = state.getParameter(paramId)) {
    slider_.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
  }
}

void Knob::resized() {
  auto r = getLocalBounds();
  const int height = juce::jmin(r.getHeight(), r.getWidth() + 16 + 16);
  r = r.withSizeKeepingCentre(r.getWidth(), height);
  labelArea_ = r.removeFromTop(16);
  slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, true, juce::jmax(r.getWidth(), 70), 16);
  slider_.setBounds(r);
}

void Knob::paint(juce::Graphics& g) {
  g.setColour(colours::text);
  g.setFont(uiFont(12.0f, true));
  g.drawText(name_, labelArea_, juce::Justification::centred);
}

// --- SegmentDisplay ---------------------------------------------------------------------

void SegmentDisplay::setState(const tresses::DisplayState& state) {
  if (std::memcmp(state.text, state_.text, 4) != 0 || state.brightness != state_.brightness) {
    state_ = state;
    repaint();
  }
}

namespace {

// Segment = hexagone allongé entre deux points, extrémités en pointe.
juce::Path segmentPath(juce::Point<float> a, juce::Point<float> b, float thickness, float inset) {
  juce::Path p;
  const auto dir = b - a;
  const float length = dir.getDistanceFromOrigin();
  if (length <= 2.0f * inset) return p;
  const auto u = dir / length;
  const juce::Point<float> v(-u.y, u.x);
  a += u * inset;
  b -= u * inset;
  const float hw = thickness * 0.5f;
  p.startNewSubPath(a);
  p.lineTo(a + u * hw + v * hw);
  p.lineTo(b - u * hw + v * hw);
  p.lineTo(b);
  p.lineTo(b - u * hw - v * hw);
  p.lineTo(a + u * hw - v * hw);
  p.closeSubPath();
  return p;
}

}  // namespace

void SegmentDisplay::paintDigit(juce::Graphics& g, juce::Rectangle<float> cell, uint16_t segments,
                                float intensity) const {
  const float w = cell.getWidth();
  const float t = w * 0.125f;
  const float L = cell.getX() + t * 0.5f;
  const float R = cell.getRight() - t * 0.5f;
  const float T = cell.getY() + t * 0.5f;
  const float B = cell.getBottom() - t * 0.5f;
  const float M = (T + B) * 0.5f;
  const float C = (L + R) * 0.5f;
  const float straightInset = t * 0.6f;
  const float diagonalInset = t * 1.15f;

  struct Seg { uint16_t mask; juce::Point<float> a, b; bool diagonal; };
  const Seg segs[] = {
    { 0x8000, { L, T }, { R, T }, false },  // a
    { 0x4000, { R, T }, { R, M }, false },  // b
    { 0x2000, { R, M }, { R, B }, false },  // c
    { 0x1000, { L, B }, { R, B }, false },  // d
    { 0x0800, { L, M }, { L, B }, false },  // e
    { 0x0400, { L, T }, { L, M }, false },  // f
    { 0x0200, { L, T }, { C, M }, true },   // g
    { 0x0100, { C, T }, { C, M }, false },  // h
    { 0x0080, { R, T }, { C, M }, true },   // j
    { 0x0040, { C, M }, { R, M }, false },  // k
    { 0x0020, { R, B }, { C, M }, true },   // l
    { 0x0010, { C, M }, { C, B }, false },  // m
    { 0x0008, { L, B }, { C, M }, true },   // n
    { 0x0004, { L, M }, { C, M }, false },  // p
  };

  // Légère inclinaison, comme les afficheurs LED à segments.
  const auto slant = juce::AffineTransform::translation(-cell.getCentreX(), -cell.getCentreY())
                         .sheared(-0.1f, 0.0f)
                         .translated(cell.getCentreX(), cell.getCentreY());

  for (const auto& s : segs) {
    auto path = segmentPath(s.a, s.b, s.diagonal ? t * 0.85f : t,
                            s.diagonal ? diagonalInset : straightInset);
    path.applyTransform(slant);
    if (segments & s.mask) {
      g.setColour(colours::led.withAlpha(0.22f * intensity));
      g.strokePath(path, juce::PathStrokeType(t * 0.7f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
      g.setColour(colours::led.withMultipliedBrightness(0.55f + 0.45f * intensity));
      g.fillPath(path);
    } else {
      g.setColour(colours::led.withAlpha(0.07f));
      g.fillPath(path);
    }
  }
}

void SegmentDisplay::paint(juce::Graphics& g) {
  auto b = getLocalBounds().toFloat();
  g.setColour(juce::Colour(0xff09090a));
  g.fillRoundedRectangle(b, 6.0f);
  g.setColour(juce::Colour(0xff2c2f34));
  g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);

  auto inner = b.reduced(10.0f, 9.0f);
  g.setColour(juce::Colour(0xff150807));
  g.fillRoundedRectangle(inner, 3.0f);

  // Luminosité : le driver allume l'afficheur (brightness + 1) cycles sur 4.
  const float duty = (static_cast<float>(juce::jmin<int>(state_.brightness, 3)) + 1.0f) / 4.0f;
  const float intensity = 0.3f + 0.7f * duty;
  const float cellWidth = inner.getWidth() / 4.0f;
  for (int i = 0; i < 4; ++i) {
    auto cell = juce::Rectangle<float>(inner.getX() + static_cast<float>(i) * cellWidth,
                                       inner.getY(), cellWidth, inner.getHeight())
                    .reduced(cellWidth * 0.14f, inner.getHeight() * 0.1f);
    const auto c = static_cast<uint8_t>(state_.text[i]);
    paintDigit(g, cell, font_ ? font_[c] : 0, intensity);
  }
}

// --- Encoder -----------------------------------------------------------------------------

Encoder::Encoder(TressesAudioProcessor& processor) : processor_(processor) {
  setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
  setTooltip(juce::String::fromUTF8(
      "Encodeur : glisser ou molette pour tourner, clic pour appuyer, "
      "maintenir environ 1 s pour un appui long"));
}

void Encoder::turn(int steps) {
  if (steps == 0) return;
  position_ += steps;
  processor_.encoderTurn(steps);
  repaint();
}

void Encoder::paint(juce::Graphics& g) {
  const auto b = getLocalBounds().toFloat().reduced(4.0f);
  const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
  const auto c = b.getCentre();

  // Écrou et collerette.
  g.setColour(juce::Colour(0xff0e0f11));
  g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
  const float knobR = r * (pressed_ ? 0.8f : 0.84f);
  const auto knob = juce::Rectangle<float>(2 * knobR, 2 * knobR).withCentre(c);
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.fillEllipse(knob.translated(0.0f, pressed_ ? 1.0f : 3.0f));
  g.setGradientFill(juce::ColourGradient(juce::Colour(pressed_ ? 0xff3a3f45 : 0xff50565e),
                                         knob.getX(), knob.getY(), juce::Colour(0xff191b1e),
                                         knob.getX(), knob.getBottom(), false));
  g.fillEllipse(knob);

  // Stries qui tournent avec l'encodeur (24 crans par tour).
  const float step = juce::MathConstants<float>::twoPi / 24.0f;
  g.setColour(juce::Colour(0xff6a7078));
  for (int i = 0; i < 24; ++i) {
    const float a = static_cast<float>(i + position_) * step;
    const juce::Point<float> p0(c.x + knobR * 0.78f * std::sin(a), c.y - knobR * 0.78f * std::cos(a));
    const juce::Point<float> p1(c.x + knobR * 0.96f * std::sin(a), c.y - knobR * 0.96f * std::cos(a));
    g.drawLine({ p0, p1 }, 1.2f);
  }
  const float a = static_cast<float>(position_) * step;
  const juce::Point<float> dot(c.x + knobR * 0.55f * std::sin(a), c.y - knobR * 0.55f * std::cos(a));
  g.setColour(colours::text);
  g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre(dot));
}

void Encoder::mouseDown(const juce::MouseEvent& e) {
  down_ = true;
  turning_ = false;
  dragAccumulator_ = 0.0f;
  lastDrag_ = e.position;
  downTime_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
  startTimerHz(60);
}

void Encoder::mouseDrag(const juce::MouseEvent& e) {
  if (!turning_ && e.getDistanceFromDragStart() > 4) turning_ = true;
  if (!turning_) return;
  const auto d = e.position - lastDrag_;
  lastDrag_ = e.position;
  dragAccumulator_ += d.x - d.y;
  constexpr float kPixelsPerStep = 12.0f;
  int steps = 0;
  while (dragAccumulator_ >= kPixelsPerStep) { ++steps; dragAccumulator_ -= kPixelsPerStep; }
  while (dragAccumulator_ <= -kPixelsPerStep) { --steps; dragAccumulator_ += kPixelsPerStep; }
  turn(steps);
}

void Encoder::mouseUp(const juce::MouseEvent&) {
  down_ = false;
  if (pressed_) {
    pressed_ = false;
    processor_.encoderPress(false);
  } else if (!turning_) {
    // Clic bref : bouton enfoncé 60 ms, le temps que l'anti-rebond du firmware le voie.
    pressed_ = true;
    processor_.encoderPress(true);
    releaseAt_ = juce::Time::getMillisecondCounterHiRes() * 0.001 + 0.06;
  }
  repaint();
}

void Encoder::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) {
  const float delta = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
  if (!wheel.isSmooth) {
    // Molette crantée : un cran de molette = un cran d'encodeur.
    if (delta > 0.0f) turn(1);
    else if (delta < 0.0f) turn(-1);
    return;
  }
  // Pavé tactile : défilement continu.
  wheelAccumulator_ += delta * 12.0f;
  int steps = 0;
  while (wheelAccumulator_ >= 1.0f) { ++steps; wheelAccumulator_ -= 1.0f; }
  while (wheelAccumulator_ <= -1.0f) { --steps; wheelAccumulator_ += 1.0f; }
  turn(steps);
}

void Encoder::timerCallback() {
  const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
  if (down_ && !turning_ && !pressed_ && now - downTime_ > 0.25) {
    // Maintenu sans tourner : le bouton est enfoncé jusqu'au relâchement.
    pressed_ = true;
    processor_.encoderPress(true);
    repaint();
  }
  if (!down_ && pressed_ && releaseAt_ > 0.0 && now >= releaseAt_) {
    pressed_ = false;
    releaseAt_ = 0.0;
    processor_.encoderPress(false);
    repaint();
  }
  if (!down_ && !pressed_) stopTimer();
}

// --- Jack ----------------------------------------------------------------------------------

Jack::Jack(const juce::String& name, bool output) : name_(name), output_(output) {}

void Jack::setLevel(float level) {
  level = juce::jlimit(0.0f, 1.0f, level);
  if (std::abs(level - level_) > 0.02f) {
    level_ = level;
    repaint();
  }
}

void Jack::paint(juce::Graphics& g) {
  auto b = getLocalBounds().toFloat();
  auto label = b.removeFromTop(16.0f);
  g.setColour(output_ ? colours::knob : colours::text);
  g.setFont(uiFont(11.0f, true));
  g.drawText(name_, label, juce::Justification::centred);

  // LED d'activité.
  const auto led = juce::Rectangle<float>(6.0f, 6.0f).withCentre({ b.getCentreX(), b.getY() + 6.0f });
  const auto ledColour = output_ ? colours::knob : colours::cable;
  if (level_ > 0.02f) {
    g.setColour(ledColour.withAlpha(0.3f * level_));
    g.fillEllipse(led.expanded(3.0f));
  }
  g.setColour(juce::Colour(0xff26292d).interpolatedWith(ledColour, level_));
  g.fillEllipse(led);
  b.removeFromTop(14.0f);

  // Prise.
  const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.36f;
  const auto c = juce::Point<float>(b.getCentreX(), b.getY() + r + 2.0f);
  const auto nut = juce::Rectangle<float>(2 * r, 2 * r).withCentre(c);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb9bec4), nut.getX(), nut.getY(),
                                         juce::Colour(0xff6c7178), nut.getX(), nut.getBottom(),
                                         false));
  g.fillEllipse(nut);
  g.setColour(juce::Colour(0xff050506));
  g.fillEllipse(nut.reduced(r * 0.42f));

  if (patched_) {
    const auto colour = output_ ? colours::knob : colours::cable;
    const auto plug = nut.reduced(r * 0.18f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(plug.translated(0.0f, 2.0f));
    g.setColour(colour.darker(0.25f));
    g.fillEllipse(plug);
    g.setColour(colour.brighter(0.2f));
    g.drawEllipse(plug.reduced(1.0f), 1.5f);
    g.setColour(colour.darker(0.6f));
    g.fillEllipse(plug.reduced(r * 0.45f));
  }
}

// --- SourceSelect -------------------------------------------------------------------------

SourceSelect::SourceSelect(juce::AudioProcessorValueTreeState& state, const char* paramId,
                           const juce::String& jackName, const juce::StringArray& items,
                           const juce::String& tooltip)
    : jackName_(jackName) {
  box_.addItemList(items, 1);
  box_.setTooltip(tooltip);
  addAndMakeVisible(box_);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
      state, paramId, box_);
}

void SourceSelect::resized() {
  auto r = getLocalBounds();
  r.removeFromTop(18);
  box_.setBounds(r.removeFromTop(28));
}

void SourceSelect::paint(juce::Graphics& g) {
  auto top = getLocalBounds().removeFromTop(18).toFloat();
  g.setColour(colours::cable);
  g.setFont(uiFont(12.0f, true));
  g.drawText(jackName_ + juce::String::fromUTF8("  \xe2\x86\x90"), top, juce::Justification::centredLeft);
}

}  // namespace ui

// --- Éditeur ---------------------------------------------------------------------------------

TressesAudioProcessorEditor::TressesAudioProcessorEditor(TressesAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), encoder_(p) {
  using namespace ui;
  setLookAndFeel(&lookAndFeel_);
  auto& state = processor_.apvts;
  auto tip = [](const char* s) { return juce::String::fromUTF8(s); };

  display_.setFont(processor_.font());
  addAndMakeVisible(display_);
  addAndMakeVisible(encoder_);

  fine_ = std::make_unique<Knob>(state, ids::fine, "FINE", colours::knob,
                                 tip("Accord fin : plus ou moins 1 demi-ton"));
  coarse_ = std::make_unique<Knob>(state, ids::coarse, "COARSE", colours::knob,
                                   tip("Accord grossier : plus ou moins 4 octaves (selon RANG)"));
  fm_ = std::make_unique<Knob>(state, ids::fmAtt, "FM", colours::attenuverter,
                               tip("Atténuverseur de l'entrée FM"));
  timbre_ = std::make_unique<Knob>(state, ids::timbre, "TIMBRE", colours::knob,
                                   tip("TIMBRE : premier paramètre du modèle"));
  modulation_ = std::make_unique<Knob>(state, ids::modulation, "MODULATION", colours::attenuverter,
                                       tip("Atténuverseur de l'entrée CV TIMBRE"));
  color_ = std::make_unique<Knob>(state, ids::color, "COLOR", colours::knob,
                                  tip("COLOR : second paramètre du modèle"));
  for (auto* k : { fine_.get(), coarse_.get(), fm_.get(), timbre_.get(), modulation_.get(), color_.get() })
    addAndMakeVisible(*k);

  jackTrig_ = std::make_unique<Jack>("TRIG", false);
  jackVoct_ = std::make_unique<Jack>("V/OCT", false);
  jackFm_ = std::make_unique<Jack>("FM", false);
  jackTimbre_ = std::make_unique<Jack>("TIMBRE", false);
  jackColor_ = std::make_unique<Jack>("COLOR", false);
  jackOut_ = std::make_unique<Jack>("OUT", true);
  for (auto* j : { jackTrig_.get(), jackVoct_.get(), jackFm_.get(), jackTimbre_.get(),
                   jackColor_.get(), jackOut_.get() })
    addAndMakeVisible(*j);
  jackOut_->setPatched(true);
  jackVoct_->setPatched(true);

  const juce::StringArray sources {
    tip("Rien"), tip("Molette"), tip("Vélocité"), tip("Aftertouch"), tip("Automation") };
  trigSource_ = std::make_unique<SourceSelect>(
      state, ids::trigSource, "TRIG", juce::StringArray { "Notes MIDI", "Rien" },
      tip("Un front sur TRIG à chaque note jouée (notes liées comprises)"));
  fmSource_ = std::make_unique<SourceSelect>(state, ids::fmSource, "FM", sources,
                                             tip("Ce qui est branché dans l'entrée FM"));
  timbreSource_ = std::make_unique<SourceSelect>(
      state, ids::timbreSource, "TIMBRE", sources,
      tip("Ce qui est branché dans l'entrée CV TIMBRE (dosé par MODULATION)"));
  colorSource_ = std::make_unique<SourceSelect>(state, ids::colorSource, "COLOR", sources,
                                                tip("Ce qui est branché dans l'entrée CV COLOR"));
  fmCv_ = std::make_unique<Knob>(state, ids::fmCv, "CV AUTO", colours::cable,
                                 tip("Tension envoyée sur FM quand la source est Automation"));
  timbreCv_ = std::make_unique<Knob>(state, ids::timbreCv, "CV AUTO", colours::cable,
                                     tip("Tension envoyée sur TIMBRE quand la source est Automation"));
  colorCv_ = std::make_unique<Knob>(state, ids::colorCv, "CV AUTO", colours::cable,
                                    tip("Tension envoyée sur COLOR quand la source est Automation"));
  bendRange_ = std::make_unique<Knob>(state, ids::bendRange, "BEND", colours::cable,
                                      tip("Amplitude du pitch bend ajouté à V/OCT"));
  for (juce::Component* c : { static_cast<juce::Component*>(trigSource_.get()),
                              static_cast<juce::Component*>(fmSource_.get()),
                              static_cast<juce::Component*>(timbreSource_.get()),
                              static_cast<juce::Component*>(colorSource_.get()),
                              static_cast<juce::Component*>(fmCv_.get()),
                              static_cast<juce::Component*>(timbreCv_.get()),
                              static_cast<juce::Component*>(colorCv_.get()),
                              static_cast<juce::Component*>(bendRange_.get()) })
    addAndMakeVisible(*c);

  setSize(780, 712);
  startTimerHz(30);
}

TressesAudioProcessorEditor::~TressesAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

void TressesAudioProcessorEditor::paint(juce::Graphics& g) {
  using namespace ui;
  g.fillAll(colours::background);

  // Façade du module.
  auto m = moduleArea_.toFloat();
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff25292e), m.getX(), m.getY(),
                                         juce::Colour(0xff1a1d20), m.getX(), m.getBottom(), false));
  g.fillRoundedRectangle(m, 8.0f);
  g.setColour(colours::moduleEdge);
  g.drawRoundedRectangle(m.reduced(0.5f), 8.0f, 1.0f);
  for (auto corner : { juce::Point<float>(m.getX() + 14, m.getY() + 14),
                       juce::Point<float>(m.getRight() - 14, m.getY() + 14),
                       juce::Point<float>(m.getX() + 14, m.getBottom() - 14),
                       juce::Point<float>(m.getRight() - 14, m.getBottom() - 14) }) {
    g.setColour(juce::Colour(0xff0d0e10));
    g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(corner));
    g.setColour(juce::Colour(0xff3d4248));
    g.drawEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(corner), 1.0f);
  }
  auto header = moduleArea_.reduced(30, 0).withTrimmedTop(14).withHeight(26);
  g.setColour(colours::text);
  g.setFont(uiFont(22.0f, true));
  g.drawText(juce::String(JucePlugin_Name).toUpperCase(), header, juce::Justification::centredLeft);
  g.setColour(colours::textDim);
  g.setFont(uiFont(11.0f));
  g.drawText(juce::String::fromUTF8("module virtuel · " TRESSES_FIRMWARE_LABEL),
             header.translated(0, 24).withHeight(16), juce::Justification::centredLeft);

  if (!processor_.hasModule()) {
    g.setColour(colours::led);
    g.setFont(uiFont(13.0f, true));
    g.drawFittedText(juce::String::fromUTF8("Trop de Tresses ouverts en même temps : "
                                            "ferme une instance puis recharge celle-ci."),
                     display_.getBounds().translated(0, display_.getHeight() + 4).withHeight(40),
                     juce::Justification::centred, 2);
  }

  // Interface MIDI -> CV.
  auto i = interfaceArea_.toFloat();
  g.setColour(colours::interface);
  g.fillRoundedRectangle(i, 8.0f);
  g.setColour(colours::moduleEdge);
  g.drawRoundedRectangle(i.reduced(0.5f), 8.0f, 1.0f);
  auto ih = interfaceArea_.reduced(20, 0).withTrimmedTop(16);
  g.setColour(colours::cable);
  g.setFont(uiFont(14.0f, true));
  g.drawText(juce::String::fromUTF8("INTERFACE MIDI \xe2\x86\x92 CV"), ih.removeFromTop(22),
             juce::Justification::centredLeft);
  g.setColour(colours::textDim);
  g.setFont(uiFont(11.5f));
  g.drawFittedText(juce::String::fromUTF8("Hors module : ce que ton clavier et Reason "
                                          "branchent dans les jacks."),
                   ih.removeFromTop(32), juce::Justification::topLeft, 2);

  // V/OCT : fixe.
  auto voctRow = juce::Rectangle<int>(interfaceArea_.getX() + 20, interfaceArea_.getY() + 156, 160, 60);
  g.setColour(colours::cable);
  g.setFont(uiFont(12.0f, true));
  g.drawText(juce::String::fromUTF8("V/OCT  \xe2\x86\x90"), voctRow.removeFromTop(18),
             juce::Justification::centredLeft);
  g.setColour(colours::text);
  g.setFont(uiFont(12.5f));
  g.drawFittedText(juce::String::fromUTF8("Notes MIDI + pitch bend\n0 V = C1, 1 V/octave"),
                   voctRow, juce::Justification::topLeft, 2);

  auto help = interfaceArea_.reduced(20, 0).withTop(interfaceArea_.getY() + 548).withHeight(112);
  g.setColour(colours::textDim);
  g.setFont(uiFont(11.5f));
  g.drawFittedText(juce::String::fromUTF8(
                       "Encodeur : glisser ou molette pour tourner, clic pour appuyer, "
                       "maintenir environ 1 s pour un appui long.\n"
                       "Comme le module, Tresses sonne en continu : active |\\VCA "
                       "(enveloppe AD) dans le menu pour jouer des notes détachées.\n"
                       "Les réglages du menu sont sauvés dans la flash du module, "
                       "elle-même enregistrée dans ton projet."),
                   help, juce::Justification::topLeft, 9);

  auto footer = getLocalBounds().removeFromBottom(22).reduced(18, 0);
  g.setColour(colours::textDim.withAlpha(0.7f));
  g.setFont(uiFont(10.5f));
  g.drawText(juce::String::fromUTF8(TRESSES_FIRMWARE_CREDIT
                                    " Tresses n'est pas un produit Mutable Instruments."),
             footer, juce::Justification::centredLeft);
}

void TressesAudioProcessorEditor::resized() {
  moduleArea_ = { 16, 16, 432, 670 };
  interfaceArea_ = { 464, 16, 300, 670 };

  const int mx = moduleArea_.getX();
  const int my = moduleArea_.getY();

  // Afficheur et encodeur.
  display_.setBounds(mx + 28, my + 70, 260, 106);
  encoder_.setBounds(mx + 310, my + 76, 96, 96);

  // Potards : deux rangées de trois.
  const int colWidth = (moduleArea_.getWidth() - 40) / 3;
  auto column = [&](int col, int y, int width, int height) {
    const int cx = mx + 20 + colWidth * col + colWidth / 2;
    return juce::Rectangle<int>(cx - width / 2, y, width, height);
  };
  fine_->setBounds(column(0, my + 208, 92, 140));
  coarse_->setBounds(column(1, my + 192, 130, 166));
  fm_->setBounds(column(2, my + 208, 92, 140));
  timbre_->setBounds(column(0, my + 376, 130, 166));
  modulation_->setBounds(column(1, my + 392, 92, 140));
  color_->setBounds(column(2, my + 376, 130, 166));

  // Jacks.
  const int jackWidth = (moduleArea_.getWidth() - 40) / 6;
  int jx = mx + 20;
  for (auto* j : { jackTrig_.get(), jackVoct_.get(), jackFm_.get(), jackTimbre_.get(),
                   jackColor_.get(), jackOut_.get() }) {
    j->setBounds(jx, my + 566, jackWidth, 84);
    jx += jackWidth;
  }

  // Interface MIDI -> CV.
  const int ix = interfaceArea_.getX() + 20;
  const int iy = interfaceArea_.getY();
  trigSource_->setBounds(ix, iy + 84, 160, 48);
  bendRange_->setBounds(ix + 178, iy + 136, 84, 96);
  fmSource_->setBounds(ix, iy + 254, 160, 48);
  fmCv_->setBounds(ix + 178, iy + 234, 84, 96);
  timbreSource_->setBounds(ix, iy + 352, 160, 48);
  timbreCv_->setBounds(ix + 178, iy + 332, 84, 96);
  colorSource_->setBounds(ix, iy + 450, 160, 48);
  colorCv_->setBounds(ix + 178, iy + 430, 84, 96);
}

void TressesAudioProcessorEditor::timerCallback() {
  display_.setState(processor_.display());

  const auto meters = processor_.jackMeters();
  jackTrig_->setLevel(meters.trig);
  jackVoct_->setLevel(std::abs(meters.voct) / 6.0f);
  jackFm_->setLevel(std::abs(meters.fm) / 5.0f);
  jackTimbre_->setLevel(meters.timbre / 5.0f);
  jackColor_->setLevel(meters.color / 5.0f);
  jackOut_->setLevel(meters.out);

  auto patched = [this](const char* id) {
    return juce::roundToInt(processor_.apvts.getRawParameterValue(id)->load()) != 0;
  };
  jackTrig_->setPatched(!patched(ids::trigSource));  // 0 = Notes MIDI
  jackFm_->setPatched(patched(ids::fmSource));
  jackTimbre_->setPatched(patched(ids::timbreSource));
  jackColor_->setPatched(patched(ids::colorSource));
}
