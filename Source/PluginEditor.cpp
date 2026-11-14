// Tresses — interface : façade du module + interface MIDI -> CV.

#include "PluginEditor.h"

#include <cmath>
#include <cstring>


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

juce::Point<float> polar(juce::Point<float> c, float r, float angle) {
  return { c.x + r * std::sin(angle), c.y - r * std::cos(angle) };
}

// Moletage : stries alternées autour d'un disque, qui tournent avec l'angle.
void knurl(juce::Graphics& g, juce::Point<float> c, float r, float angle, int count,
           juce::Colour dark, juce::Colour light) {
  g.setColour(dark);
  g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
  g.setColour(light);
  const float step = juce::MathConstants<float>::twoPi / static_cast<float>(count);
  for (int i = 0; i < count; ++i) {
    juce::Path wedge;
    const float a0 = angle + static_cast<float>(i) * step;
    wedge.addPieSegment(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c), a0, a0 + step * 0.5f, 0.0f);
    g.fillPath(wedge);
  }
}

// Potard « à jupe colorée » (modèle C2) : jupe noire avec repère blanc, anneau de
// couleur, corps moleté, dessus noir avec un point de couleur.
// `encoder` : l'encodeur EDIT, même dessin mais anneau gris et point blanc.
void drawSkirtKnob(juce::Graphics& g, juce::Point<float> c, float r, float angle,
                   juce::Colour cap, bool encoder, bool pressed = false) {
  const float scale = pressed ? 0.96f : 1.0f;
  // Ombre portée.
  g.setColour(juce::Colours::black.withAlpha(0.32f));
  g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c.translated(0.0f, pressed ? 1.0f : 3.0f)));
  // Jupe.
  if (encoder) {
    knurl(g, c, r, angle, 44, juce::Colour(0xff0d0c0b), juce::Colour(0xff34332f));
  } else {
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3b3a37), c.x - r * 0.3f, c.y - r * 0.4f,
                                           juce::Colour(0xff141312), c.x + r * 0.5f, c.y + r * 0.8f, true));
    g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
    g.setColour(colours::capWhite);
    g.drawLine({ polar(c, r * 0.80f, angle), polar(c, r * 0.98f, angle) }, juce::jmax(2.0f, r * 0.07f));
  }
  r *= scale;
  // Anneau de couleur.
  const float ring = r * 0.74f;
  g.setColour(juce::Colours::black.withAlpha(0.45f));
  g.fillEllipse(juce::Rectangle<float>(2 * ring, 2 * ring).withCentre(c.translated(0.0f, 2.0f)));
  g.setColour(encoder ? juce::Colour(0xff2c2d30) : cap);
  g.fillEllipse(juce::Rectangle<float>(2 * ring, 2 * ring).withCentre(c));
  // Corps moleté, puis dessus.
  knurl(g, c, r * 0.66f, angle, 36, juce::Colour(0xff0d0c0b), juce::Colour(0xff2b2a27));
  const float top = r * 0.5f;
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2f2e2b), c.x - top * 0.3f, c.y - top * 0.4f,
                                         juce::Colour(0xff0f0e0d), c.x + top, c.y + top, true));
  g.fillEllipse(juce::Rectangle<float>(2 * top, 2 * top).withCentre(c));
  // Point repère.
  const float dot = juce::jmax(3.0f, r * 0.09f);
  g.setColour(encoder ? colours::capWhite : cap);
  g.fillEllipse(juce::Rectangle<float>(2 * dot, 2 * dot).withCentre(polar(c, r * 0.37f, angle)));
}

// Nom d'une commande : texte noir, ou blanc dans une étiquette de couleur.
void drawName(juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area,
              juce::Colour box, juce::Colour plain, float size = 12.0f) {
  g.setFont(uiFont(size, true));
  if (box.isTransparent()) {
    g.setColour(plain);
    g.drawText(name, area, juce::Justification::centred);
    return;
  }
  const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), name) + 14.0f;
  const auto pill = juce::Rectangle<float>(w, size + 7.0f).withCentre(area.getCentre());
  g.setColour(box);
  g.fillRoundedRectangle(pill, 3.0f);
  g.setColour(juce::Colours::white);
  g.drawText(name, pill, juce::Justification::centred);
}

}  // namespace

// --- LookAndFeel -------------------------------------------------------------------

LookAndFeel::LookAndFeel() {
  setColour(juce::ResizableWindow::backgroundColourId, colours::background);
  setColour(juce::Slider::textBoxTextColourId, colours::inkDim);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
  setColour(juce::ComboBox::backgroundColourId, colours::field);
  setColour(juce::ComboBox::textColourId, colours::text);
  setColour(juce::ComboBox::outlineColourId, colours::moduleEdge);
  setColour(juce::ComboBox::arrowColourId, colours::textDim);
  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff26292e));
  setColour(juce::PopupMenu::textColourId, colours::text);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::tealLabel);
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
  setColour(juce::TextButton::textColourOffId, colours::ink);
  setColour(juce::TextButton::textColourOnId, colours::ink);
  setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff1f2125));
  setColour(juce::TooltipWindow::textColourId, colours::text);
  setColour(juce::TooltipWindow::outlineColourId, colours::moduleEdge);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPos, float startAngle, float endAngle,
                                   juce::Slider& slider) {
  const auto area = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
  const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
  const float angle = startAngle + sliderPos * (endAngle - startAngle);
  drawSkirtKnob(g, area.getCentre(), radius, angle, accentOf(slider, colours::capWhite), false);
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
juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int) { return uiFont(12.0f); }

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                       bool highlighted, bool down) {
  // Bouton « Firmware » : pastille posée sur l'aluminium.
  const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
  const float r = bounds.getHeight() * 0.5f;
  g.setColour(juce::Colours::white.withAlpha(down ? 0.35f : (highlighted ? 0.8f : 0.55f)));
  g.fillRoundedRectangle(bounds, r);
  g.setColour(juce::Colour(0xffb9b7b1));
  g.drawRoundedRectangle(bounds, r, 1.0f);
}

juce::Label* LookAndFeel::createSliderTextBox(juce::Slider& slider) {
  auto* label = LookAndFeel_V4::createSliderTextBox(slider);
  label->setFont(uiFont(12.0f));
  label->setColour(juce::Label::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
  label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
  label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
  return label;
}

// --- Knob ------------------------------------------------------------------------------

Knob::Knob(juce::AudioProcessorValueTreeState& state, const char* paramId,
           const juce::String& name, juce::Colour cap, const juce::String& tooltip,
           juce::Colour labelBox, bool onDark)
    : name_(name), labelBox_(labelBox), onDark_(onDark) {
  slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  slider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  slider_.setTooltip(tooltip);
  setAccent(slider_, cap);
  slider_.onValueChange = [this] { repaint(valueArea_); };
  addAndMakeVisible(slider_);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      state, paramId, slider_);
  if (auto* param = state.getParameter(paramId)) {
    slider_.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
  }
}

void Knob::resized() {
  auto r = getLocalBounds();
  valueArea_ = r.removeFromBottom(16);
  labelArea_ = r.removeFromBottom(22);
  const int size = juce::jmin(r.getWidth(), r.getHeight());
  slider_.setBounds(r.withSizeKeepingCentre(size, size));
}

void Knob::paint(juce::Graphics& g) {
  drawName(g, name_, labelArea_.toFloat(), labelBox_, onDark_ ? colours::text : colours::ink);
  g.setColour(onDark_ ? colours::textDim : colours::inkDim);
  g.setFont(uiFont(11.0f));
  g.drawText(slider_.getTextFromValue(slider_.getValue()), valueArea_, juce::Justification::centred);
}

// --- SegmentDisplay ---------------------------------------------------------------------

void SegmentDisplay::setState(const tresses::emu::BraidsMachine::Display& state) {
  if (std::memcmp(state.segments, state_.segments, sizeof(state.segments)) != 0 ||
      std::abs(state.brightness - state_.brightness) > 0.02f) {
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
  // Fenêtre en verre fumé, enfoncée dans l'aluminium.
  auto b = getLocalBounds().toFloat();
  g.setColour(juce::Colours::white.withAlpha(0.7f));
  g.fillRoundedRectangle(b.translated(0.0f, 1.0f), 8.0f);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3d3f3e), 0.0f, b.getY(),
                                         juce::Colour(0xff2a2c2b), 0.0f, b.getBottom(), false));
  g.fillRoundedRectangle(b, 8.0f);
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.drawRoundedRectangle(b.reduced(1.0f), 8.0f, 2.0f);

  auto inner = b.reduced(10.0f, 9.0f);
  g.setColour(juce::Colour(0xff171817));
  g.fillRoundedRectangle(inner, 3.0f);

  // Luminosité : part du temps pendant laquelle le firmware allume l'afficheur.
  const float intensity = state_.brightness <= 0.0f ? 0.0f : 0.3f + 0.7f * state_.brightness;
  const float cellWidth = inner.getWidth() / 4.0f;
  for (int i = 0; i < 4; ++i) {
    auto cell = juce::Rectangle<float>(inner.getX() + static_cast<float>(i) * cellWidth,
                                       inner.getY(), cellWidth, inner.getHeight())
                    .reduced(cellWidth * 0.14f, inner.getHeight() * 0.1f);
    paintDigit(g, cell, intensity > 0.0f ? state_.segments[i] : 0, intensity);
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
  const auto b = getLocalBounds().toFloat().reduced(5.0f);
  const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
  // 24 crans par tour : le moletage et le point tournent avec l'encodeur.
  const float angle = static_cast<float>(position_) * juce::MathConstants<float>::twoPi / 24.0f;
  drawSkirtKnob(g, b.getCentre(), r, angle, colours::capWhite, true, pressed_);
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

Jack::Jack(const juce::String& name, bool output, juce::Colour labelBox)
    : name_(name), output_(output), labelBox_(labelBox) {}

void Jack::setLevel(float level) {
  level = juce::jlimit(0.0f, 1.0f, level);
  if (std::abs(level - level_) > 0.02f) {
    level_ = level;
    repaint();
  }
}

void Jack::paint(juce::Graphics& g) {
  auto b = getLocalBounds().toFloat();
  auto label = b.removeFromTop(20.0f);
  // OUT : texte blanc sur la case ardoise que dessine l'éditeur.
  drawName(g, name_, label, labelBox_, output_ ? juce::Colours::white : colours::ink, 11.0f);

  // LED d'activité.
  const auto led = juce::Rectangle<float>(5.0f, 5.0f).withCentre({ b.getCentreX(), b.getY() + 6.0f });
  const auto ledColour = output_ ? colours::magenta.brighter(0.4f) : colours::teal;
  if (level_ > 0.02f) {
    g.setColour(ledColour.withAlpha(0.3f * level_));
    g.fillEllipse(led.expanded(3.0f));
  }
  g.setColour(juce::Colour(0xff8f8d87).interpolatedWith(ledColour, level_));
  g.fillEllipse(led);
  b.removeFromTop(14.0f);

  // Prise : écrou chromé, trou noir.
  const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.36f;
  const auto c = juce::Point<float>(b.getCentreX(), b.getY() + r + 2.0f);
  const auto nut = juce::Rectangle<float>(2 * r, 2 * r).withCentre(c);
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.fillEllipse(nut.translated(0.0f, 2.0f));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe9e8e4), nut.getX(), nut.getY(),
                                         juce::Colour(0xff8f8d87), nut.getRight(), nut.getBottom(),
                                         false));
  g.fillEllipse(nut);
  g.setColour(juce::Colour(0xff050505));
  g.fillEllipse(nut.reduced(r * 0.46f));

  if (patched_) {
    const auto colour = output_ ? colours::magenta : colours::tealLabel;
    const auto plug = nut.reduced(r * 0.18f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(plug.translated(0.0f, 2.0f));
    g.setColour(colour.darker(0.2f));
    g.fillEllipse(plug);
    g.setColour(colour.brighter(0.3f));
    g.drawEllipse(plug.reduced(1.0f), 1.5f);
    g.setColour(colour.darker(0.7f));
    g.fillEllipse(plug.reduced(r * 0.45f));
  }
}

// --- SourceSelect -------------------------------------------------------------------------

SourceSelect::SourceSelect(juce::AudioProcessorValueTreeState& state, const char* paramId,
                           const juce::String& jackName, const juce::StringArray& items,
                           const juce::String& tooltip, juce::Colour labelBox)
    : jackName_(jackName), labelBox_(labelBox) {
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
  g.setFont(uiFont(11.0f, true));
  const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), jackName_) + 14.0f;
  drawName(g, jackName_, top.withWidth(w), labelBox_, colours::text, 11.0f);
  g.setColour(colours::textDim);
  g.drawText(juce::String::fromUTF8("\xe2\x86\x90"), top.withTrimmedLeft(w + 2.0f),
             juce::Justification::centredLeft);
}

}  // namespace ui

// --- Éditeur ---------------------------------------------------------------------------------

TressesAudioProcessorEditor::TressesAudioProcessorEditor(TressesAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), encoder_(p) {
  using namespace ui;
  setLookAndFeel(&lookAndFeel_);
  auto& state = processor_.apvts;
  auto tip = [](const char* s) { return juce::String::fromUTF8(s); };

  addAndMakeVisible(display_);
  addAndMakeVisible(encoder_);

  firmwareButton_.setTooltip(tip("Changer de firmware : celui fourni avec Tresses, ou un fichier .bin ou .wav"));
  firmwareButton_.onClick = [this] { showFirmwareMenu(); };
  addAndMakeVisible(firmwareButton_);
  refreshFirmwareButton();

  fine_ = std::make_unique<Knob>(state, ids::fine, "FINE", colours::capWhite,
                                 tip("Accord fin : plus ou moins 1 demi-ton"));
  coarse_ = std::make_unique<Knob>(state, ids::coarse, "COARSE", colours::capWhite,
                                   tip("Accord grossier : plus ou moins 4 octaves (selon RANG)"));
  fm_ = std::make_unique<Knob>(state, ids::fmAtt, "FM", colours::capWhite,
                               tip("Atténuverseur de l'entrée FM"));
  timbre_ = std::make_unique<Knob>(state, ids::timbre, "TIMBRE", colours::teal,
                                   tip("TIMBRE : premier paramètre du modèle"), colours::tealLabel);
  modulation_ = std::make_unique<Knob>(state, ids::modulation, "MODULATION", colours::teal,
                                       tip("Atténuverseur de l'entrée CV TIMBRE"), colours::tealLabel);
  color_ = std::make_unique<Knob>(state, ids::color, "COLOR", colours::magenta,
                                  tip("COLOR : second paramètre du modèle"), colours::magenta);
  for (auto* k : { fine_.get(), coarse_.get(), fm_.get(), timbre_.get(), modulation_.get(), color_.get() })
    addAndMakeVisible(*k);

  jackTrig_ = std::make_unique<Jack>("TRIG", false);
  jackVoct_ = std::make_unique<Jack>("V/OCT", false);
  jackFm_ = std::make_unique<Jack>("FM", false);
  jackTimbre_ = std::make_unique<Jack>("TIMBRE", false, colours::tealLabel);
  jackColor_ = std::make_unique<Jack>("COLOR", false, colours::magenta);
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
      tip("Ce qui est branché dans l'entrée CV TIMBRE (dosé par MODULATION)"), colours::tealLabel);
  colorSource_ = std::make_unique<SourceSelect>(state, ids::colorSource, "COLOR", sources,
                                                tip("Ce qui est branché dans l'entrée CV COLOR"), colours::magenta);
  fmCv_ = std::make_unique<Knob>(state, ids::fmCv, "CV AUTO", colours::capWhite,
                                 tip("Tension envoyée sur FM quand la source est Automation"),
                                 juce::Colours::transparentBlack, true);
  timbreCv_ = std::make_unique<Knob>(state, ids::timbreCv, "CV AUTO", colours::teal,
                                     tip("Tension envoyée sur TIMBRE quand la source est Automation"),
                                     juce::Colours::transparentBlack, true);
  colorCv_ = std::make_unique<Knob>(state, ids::colorCv, "CV AUTO", colours::magenta,
                                    tip("Tension envoyée sur COLOR quand la source est Automation"),
                                    juce::Colours::transparentBlack, true);
  bendRange_ = std::make_unique<Knob>(state, ids::bendRange, "BEND", colours::capWhite,
                                      tip("Amplitude du pitch bend ajouté à V/OCT"),
                                      juce::Colours::transparentBlack, true);
  for (juce::Component* c : { static_cast<juce::Component*>(trigSource_.get()),
                              static_cast<juce::Component*>(fmSource_.get()),
                              static_cast<juce::Component*>(timbreSource_.get()),
                              static_cast<juce::Component*>(colorSource_.get()),
                              static_cast<juce::Component*>(fmCv_.get()),
                              static_cast<juce::Component*>(timbreCv_.get()),
                              static_cast<juce::Component*>(colorCv_.get()),
                              static_cast<juce::Component*>(bendRange_.get()) })
    addAndMakeVisible(*c);

  // Fond de la façade : courbes de niveau autour de trois sommets, comme sur la
  // maquette C2 (coordonnées relatives au coin de la façade).
  {
    juce::Random random(7);
    const juce::Point<float> peaks[] = { { 90.0f, 300.0f }, { 330.0f, 520.0f }, { 300.0f, 140.0f } };
    for (const auto& peak : peaks) {
      for (int k = 1; k < 12; ++k) {
        const float r0 = 14.0f * static_cast<float>(k);
        const float phase = random.nextFloat() * 6.0f;
        juce::Path ring;
        for (int i = 0; i <= 72; ++i) {
          const float a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / 72.0f;
          const float rr = r0 * (1.0f + 0.18f * std::sin(3.0f * a + phase + static_cast<float>(k) * 0.3f)
                                      + 0.08f * std::sin(5.0f * a + phase * 2.0f));
          const juce::Point<float> pt(peak.x + rr * std::cos(a) * 1.25f, peak.y + rr * std::sin(a));
          if (i == 0) ring.startNewSubPath(pt); else ring.lineTo(pt);
        }
        ring.closeSubPath();
        topo_.addPath(ring);
      }
    }
  }

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

  // --- Façade du module : aluminium clair, lignes topographiques.
  const auto m = moduleArea_.toFloat();
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.fillRoundedRectangle(m.translated(0.0f, 4.0f), 10.0f);
  {
    juce::Graphics::ScopedSaveState save(g);
    juce::Path face;
    face.addRoundedRectangle(m, 10.0f);
    g.reduceClipRegion(face);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffefeeec), 0.0f, m.getY(),
                                           colours::panelDark, 0.0f, m.getBottom(), false));
    g.fillRect(m);
    g.setColour(colours::topo);
    g.strokePath(topo_, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(m.getX(), m.getY()));
  }
  for (auto corner : { juce::Point<float>(m.getX() + 18, m.getY() + 18),
                       juce::Point<float>(m.getRight() - 18, m.getY() + 18),
                       juce::Point<float>(m.getX() + 18, m.getBottom() - 18),
                       juce::Point<float>(m.getRight() - 18, m.getBottom() - 18) }) {
    const auto screw = juce::Rectangle<float>(12.0f, 12.0f).withCentre(corner);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff7f7f5), screw.getX(), screw.getY(),
                                           juce::Colour(0xffa9a7a1), screw.getRight(), screw.getBottom(), false));
    g.fillEllipse(screw);
  }

  const int mx = moduleArea_.getX(), my = moduleArea_.getY();
  g.setColour(colours::ink);
  g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
  g.drawText(JucePlugin_Name, juce::Rectangle<int>(mx + 34, my + 16, 120, 30), juce::Justification::centredLeft);
  g.setColour(colours::inkDim);
  g.setFont(juce::Font(juce::FontOptions(13.0f)));
  g.drawText("macro oscillator", juce::Rectangle<int>(mx + 140, my + 24, 140, 18), juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xffc9c7c2));
  g.fillRect(mx + 24, my + 54, moduleArea_.getWidth() - 48, 1);
  g.fillRect(mx, my + 512, moduleArea_.getWidth(), 1);

  // Occupation du processeur du module (STM32F103 à 72 MHz) : texte et barre.
  {
    const bool crashed = processor_.moduleCrashed();
    const float load = juce::jlimit(0.0f, 1.0f, processor_.moduleLoad());
    const int percent = juce::roundToInt(load * 100.0f);
    const bool hot = crashed || percent >= 95;
    const auto text = juce::Rectangle<int>(mx + 280, my + 20, 124, 14);
    g.setColour(hot ? colours::magenta : colours::inkDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f, crashed ? juce::Font::bold : juce::Font::plain)));
    g.drawText(crashed ? juce::String::fromUTF8("firmware planté")
                       : juce::String::fromUTF8("processeur ") + juce::String(percent) + " %",
               text, juce::Justification::centredLeft);
    const auto bar = juce::Rectangle<float>(static_cast<float>(mx + 280), static_cast<float>(my + 37), 124.0f, 4.0f);
    g.setColour(juce::Colour(0xffcfcdc8));
    g.fillRoundedRectangle(bar, 2.0f);
    g.setColour(hot ? colours::magenta : colours::teal);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * (crashed ? 1.0f : load)), 2.0f);
  }

  // Nom de l'encodeur.
  g.setColour(colours::ink);
  g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
  g.drawText("EDIT", encoder_.getBounds().withY(encoder_.getBottom() - 2).withHeight(18),
             juce::Justification::centred);

  // Case OUT, ardoise.
  {
    const auto out = jackOut_->getBounds().toFloat().expanded(4.0f, 6.0f);
    g.setColour(colours::slate);
    g.fillRoundedRectangle(out, 6.0f);
  }

  g.setColour(colours::inkDim);
  g.setFont(juce::Font(juce::FontOptions(10.5f)));
  g.drawText(juce::String::fromUTF8("Module Braids émulé · Braids © Emilie Gillet (licence MIT)"),
             juce::Rectangle<int>(mx, moduleArea_.getBottom() - 30, moduleArea_.getWidth(), 14),
             juce::Justification::centred);

  // --- Interface MIDI -> CV : panneau ardoise.
  const auto i = interfaceArea_.toFloat();
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.fillRoundedRectangle(i.translated(0.0f, 4.0f), 10.0f);
  g.setColour(colours::interface);
  g.fillRoundedRectangle(i, 10.0f);
  auto ih = interfaceArea_.reduced(20, 0).withTrimmedTop(18);
  g.setColour(colours::text);
  g.setFont(uiFont(15.0f, true));
  g.drawText(juce::String::fromUTF8("INTERFACE MIDI \xe2\x86\x92 CV"), ih.removeFromTop(22),
             juce::Justification::centredLeft);
  g.setColour(colours::textDim);
  g.setFont(uiFont(11.5f));
  g.drawFittedText(juce::String::fromUTF8("Hors module : ce que ton clavier et Reason "
                                          "branchent dans les jacks."),
                   ih.removeFromTop(32), juce::Justification::topLeft, 2);

  // V/OCT : fixe.
  auto voctRow = juce::Rectangle<int>(interfaceArea_.getX() + 20, interfaceArea_.getY() + 156, 160, 60);
  g.setColour(colours::text);
  g.setFont(uiFont(11.0f, true));
  g.drawText(juce::String::fromUTF8("V/OCT  \xe2\x86\x90"), voctRow.removeFromTop(18),
             juce::Justification::centredLeft);
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
  g.setColour(colours::textDim.withAlpha(0.8f));
  g.setFont(uiFont(10.5f));
  g.drawText(juce::String::fromUTF8("Tresses n'est pas un produit Mutable Instruments."),
             footer, juce::Justification::centredLeft);
}

void TressesAudioProcessorEditor::resized() {
  moduleArea_ = { 16, 16, 432, 670 };
  interfaceArea_ = { 464, 16, 300, 670 };

  const int mx = moduleArea_.getX();
  const int my = moduleArea_.getY();

  // Afficheur, firmware et encodeur.
  display_.setBounds(mx + 24, my + 68, 268, 96);
  firmwareButton_.setBounds(mx + 24, my + 172, 200, 22);
  encoder_.setBounds(mx + 312, my + 72, 86, 86);

  // Potards : centres et diamètres de la maquette C2 ; nom et valeur dessous.
  auto place = [&](ui::Knob& k, int cx, int cy, int diameter) {
    k.setBounds(mx + cx - 60, my + cy - diameter / 2, 120, diameter + 40);
  };
  place(*fine_, 86, 254, 82);
  place(*coarse_, 215, 254, 82);
  place(*fm_, 344, 254, 82);
  place(*timbre_, 86, 400, 88);
  place(*modulation_, 215, 400, 72);
  place(*color_, 344, 400, 88);

  // Jacks : mêmes colonnes que la maquette.
  const int jackX[] = { 44, 112, 180, 248, 316, 388 };
  int n = 0;
  for (auto* j : { jackTrig_.get(), jackVoct_.get(), jackFm_.get(), jackTimbre_.get(),
                   jackColor_.get(), jackOut_.get() }) {
    j->setBounds(mx + jackX[n++] - 32, my + 534, 64, 82);
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
  if (!firmwareButton_.getButtonText().contains(processor_.firmwareName())) refreshFirmwareButton();
  const int load = juce::roundToInt(processor_.moduleLoad() * 100.0f);
  if (load != shownLoad_) { shownLoad_ = load; repaint(moduleArea_.withHeight(56)); }

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

// --- Firmware ----------------------------------------------------------------------------------

void TressesAudioProcessorEditor::refreshFirmwareButton() {
  firmwareButton_.setButtonText(juce::String::fromUTF8("Firmware : ") + processor_.firmwareName() +
                                juce::String::fromUTF8("  \xe2\x96\xbe"));
}

void TressesAudioProcessorEditor::showFirmwareMenu() {
  juce::PopupMenu menu;
  menu.addItem(1, juce::String::fromUTF8("Braids v1.9 (fourni avec Tresses)"), true,
               processor_.firmwareIsBuiltIn());
  if (!processor_.firmwareIsBuiltIn()) menu.addItem(2, processor_.firmwareName(), true, true);
  menu.addSeparator();
  menu.addItem(3, juce::String::fromUTF8("Charger un fichier .bin ou .wav…"));
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(firmwareButton_),
                     [safe = juce::Component::SafePointer<TressesAudioProcessorEditor>(this)](int choice) {
                       if (safe == nullptr) return;
                       if (choice == 1) { safe->processor_.useBuiltInFirmware(); safe->refreshFirmwareButton(); }
                       if (choice == 3) safe->chooseFirmwareFile();
                     });
}

// Dossier proposé à l'ouverture : celui où l'installateur range les firmwares.
static juce::File firmwareFolder() {
  const juce::File installed("/Users/Shared/Tresses/Firmwares");
  if (installed.isDirectory()) return installed;
  return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

void TressesAudioProcessorEditor::chooseFirmwareFile() {
  fileChooser_ = std::make_unique<juce::FileChooser>(
      juce::String::fromUTF8("Choisir un firmware pour Braids (.bin ou .wav de mise à jour)"),
      firmwareFolder(), "*.bin;*.wav");
  fileChooser_->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [safe = juce::Component::SafePointer<TressesAudioProcessorEditor>(this)](const juce::FileChooser& chooser) {
        if (safe == nullptr) return;
        const juce::File file = chooser.getResult();
        if (file == juce::File()) return;
        const juce::String error = safe->processor_.loadFirmwareFile(file);
        safe->refreshFirmwareButton();
        const juce::String note = error.isNotEmpty() ? error : safe->processor_.firmwareNote();
        if (note.isNotEmpty()) {
          juce::AlertWindow::showMessageBoxAsync(
              error.isNotEmpty() ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon,
              error.isNotEmpty() ? juce::String::fromUTF8("Firmware non chargé") : juce::String::fromUTF8("Firmware chargé"),
              note, "OK", safe.getComponent());
        }
      });
}
