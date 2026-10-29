#include "HSVPicker.h"

#include "GradientSlider.h"

#include <algorithm>
#include <string>
#include <utility>

using namespace ftxui;

HSVPicker::HSVPicker(float h, float s, float v, int width, int height)
    : h_(h), s_(s), v_(v) {
  slider_h_ = GradientSlider(
      &h_, 0.f, 360.f, "Hue",
      [](float val) { return Utils::Colors::hsvToRgb({val, 1.f, 1.f}); },
      [](float val) { return std::to_string((int)val) + "\xC2\xB0"; }, // °
      [this] { notify(); }, width, height);

  slider_s_ = GradientSlider(
      &s_, 0.f, 100.f, "Sat",
      [this](float val) {
        return Utils::Colors::hsvToRgb({h_, val / 100.f, v_ / 100.f});
      },
      [](float val) { return std::to_string((int)val) + "%"; },
      [this] { notify(); }, width, height);

  slider_v_ = GradientSlider(
      &v_, 0.f, 100.f, "Val",
      [this](float val) {
        return Utils::Colors::hsvToRgb({h_, s_ / 100.f, val / 100.f});
      },
      [](float val) { return std::to_string((int)val) + "%"; },
      [this] { notify(); }, width, height);

  container_ = Container::Horizontal({slider_h_, slider_s_, slider_v_});
}

Color HSVPicker::color() const {
  Utils::Colors::RGB c = rgb();
  return Color::RGB(c.r, c.g, c.b);
}

void HSVPicker::values(float* h, float* s, float* v) const {
  if (h) *h = h_;
  if (s) *s = s_;
  if (v) *v = v_;
}

void HSVPicker::setH(float h) { h_ = std::clamp(h, 0.f, 360.f); notify(); }
void HSVPicker::setS(float s) { s_ = std::clamp(s, 0.f, 100.f); notify(); }
void HSVPicker::setV(float v) { v_ = std::clamp(v, 0.f, 100.f); notify(); }

void HSVPicker::setHSV(float h, float s, float v) {
  h_ = std::clamp(h, 0.f, 360.f);
  s_ = std::clamp(s, 0.f, 100.f);
  v_ = std::clamp(v, 0.f, 100.f);
  notify();
}
