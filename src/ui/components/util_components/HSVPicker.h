#pragma once
// Three gradient sliders (Hue / Saturation / Value) grouped into one widget.
// Owns the h/s/v state and exposes it as HSV or RGB. An optional onChange
// callback fires whenever a slider moves.

#include <ftxui/component/component.hpp>

#include <Utils/Colors/Colors.h>

#include <functional>

class HSVPicker {
public:
  // h: 0..360, s/v: 0..100 (percent). width/height set the size (in canvas
  // pixels) of each of the three sliders.
  explicit HSVPicker(float h = 200.f, float s = 100.f, float v = 100.f,
                     int width = 24, int height = 100);

  // The renderable/interactive component to place in your layout.
  ftxui::Component component() const { return container_; }

  // --- state access -------------------------------------------------------
  float h() const { return h_; }
  float s() const { return s_; }
  float v() const { return v_; }

  Utils::Colors::HSV hsv() const { return {h_, s_ / 100.f, v_ / 100.f}; }
  Utils::Colors::RGB rgb() const { return Utils::Colors::hsvToRgb(hsv()); }
  ftxui::Color color() const;

  // Copy the current values out into the given pointers (skip any null).
  void values(float* h, float* s, float* v) const;

  // Setters clamp to range and fire onChange so listeners stay in sync.
  void setH(float h);
  void setS(float s);
  void setV(float v);
  void setHSV(float h, float s, float v);

  // Fires after any slider changes the value.
  void onChange(std::function<void()> cb) { onChange_ = std::move(cb); }

private:
  void notify() { if (onChange_) onChange_(); }

  float h_, s_, v_;
  std::function<void()> onChange_;
  ftxui::Component slider_h_, slider_s_, slider_v_, container_;
};
