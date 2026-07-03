#include "GradientSlider.h"

#include <ftxui/dom/canvas.hpp>

#include <utility>

using namespace ftxui;

GradientSliderBase::GradientSliderBase(
    float* value, float min, float max, std::string label,
    std::function<Utils::Colors::RGB(float)> colorAt,
    std::function<std::string(float)> format, std::function<void()> onChange,
    int width, int height, Orientation orientation)
    : kW(width), kH(height), value_(value), min_(min), max_(max),
      label_(std::move(label)), colorAt_(std::move(colorAt)),
      format_(std::move(format)), onChange_(std::move(onChange)),
      orientation_(orientation) {}

Element GradientSliderBase::OnRender() {
  Canvas c(kW, kH);

  int pos = valueToPixel(*value_); // thumb position along the value axis
  int len = axisLen();

  // Paint the track. With a colorAt callback each line takes its custom
  // gradient colour; without one, fall back to a plain slider: filled on the
  // "min" side of the value, dimmed on the "max" side.
  for (int i = 0; i < len; ++i) {
    Color col;
    if (colorAt_) {
      // Vertical: i=0 is the top = max. Horizontal: i=0 is the left = min.
      float t = vertical() ? 1.f - static_cast<float>(i) / (len - 1)
                           : static_cast<float>(i) / (len - 1);
      float val = min_ + (max_ - min_) * t;
      Utils::Colors::RGB rgb = colorAt_(val);
      col = Color::RGB(rgb.r, rgb.g, rgb.b);
    } else {
      bool filled = vertical() ? i >= pos : i <= pos;
      col = filled ? Color::GrayLight : Color::GrayDark;
    }
    if (vertical())
      for (int px = 0; px < kW; ++px) c.DrawBlock(px, i, true, col);
    else
      for (int py = 0; py < kH; ++py) c.DrawBlock(i, py, true, col);
  }

  std::string txt = format_ ? format_(*value_) : std::to_string((int)*value_);

  if (vertical()) {
    // Thumb: a horizontal band across the width, snapped to the braille
    // character grid (4 px per cell) so it is always exactly one cell high.
    int ty = (pos / 4) * 4;
    if (ty < 0) ty = 0;
    if (ty > kH - 4) ty = kH - 4;
    for (int py = ty; py < ty + 4; ++py)
      for (int px = 0; px < kW; ++px)
        c.DrawBlock(px, py, true, Color::GrayLight);
    // Centre the text on the band. Count display columns (skip UTF-8
    // continuation bytes) so multibyte glyphs like '°' don't offset it.
    int cols = 0;
    for (unsigned char ch : txt)
      if ((ch & 0xC0) != 0x80) ++cols;
    int tx = (kW - cols * 2) / 2;
    if (tx < 0) tx = 0;
    c.DrawText(tx, ty, txt, Color::Black);
  } else {
    // Thumb: a vertical band across the height, snapped to the cell grid
    // (2 px per cell) so it is always exactly one cell wide.
    int tx = (pos / 2) * 2;
    if (tx < 0) tx = 0;
    if (tx > kW - 2) tx = kW - 2;
    for (int px = tx; px < tx + 2; ++px)
      for (int py = 0; py < kH; ++py)
        c.DrawBlock(px, py, true, Color::GrayLight);
  }

  auto track = canvas(std::move(c));
  if (Focused())
    track = track | focus;

  if (vertical()) {
    return vbox({
               std::move(track),
               separator(),
               text(label_) | center,
           }) |
           border | reflect(box_);
  }
  // Horizontal: value text beside the track, label underneath.
  return vbox({
             hbox({
                 std::move(track),
                 text(" " + txt) | vcenter,
             }),
             separator(),
             text(label_) | center,
         }) |
         border | reflect(box_);
}

bool GradientSliderBase::OnEvent(Event event) {
  if (!event.is_mouse())
    return false;

  auto& m = event.mouse();
  // Is the cursor inside the rendered box?
  bool inside = box_.Contain(m.x, m.y);

  if (m.button == Mouse::Left &&
      (m.motion == Mouse::Pressed || m.motion == Mouse::Moved)) {
    if (inside || captured_) {
      if (m.motion == Mouse::Pressed) {
        TakeFocus();
        captured_ = true;
      }
      // Terminal cell -> canvas pixel. box_.{x,y}_min is the top-left border;
      // +1 skips it to the first track cell. Vertical uses y (4 px/row),
      // horizontal uses x (2 px/col).
      int px;
      if (vertical())
        px = (m.y - (box_.y_min + 1)) * 4;
      else
        px = (m.x - (box_.x_min + 1)) * 2;
      float nv = pixelToValue(px);
      if (nv != *value_) {
        *value_ = nv;
        if (onChange_) onChange_();
      }
      return true;
    }
  }
  if (m.motion == Mouse::Released)
    captured_ = false;

  return false;
}

int GradientSliderBase::valueToPixel(float v) const {
  int len = axisLen();
  float t = (v - min_) / (max_ - min_); // 0..1
  // Vertical: max at top (pos 0). Horizontal: min at left (pos 0).
  float f = vertical() ? (1.f - t) : t;
  int pos = static_cast<int>(f * (len - 1) + 0.5f);
  return pos < 0 ? 0 : (pos >= len ? len - 1 : pos);
}

float GradientSliderBase::pixelToValue(int pos) const {
  int len = axisLen();
  if (pos < 0) pos = 0;
  if (pos >= len) pos = len - 1;
  float f = static_cast<float>(pos) / (len - 1);
  float t = vertical() ? (1.f - f) : f;
  float v = min_ + (max_ - min_) * t;
  return v < min_ ? min_ : (v > max_ ? max_ : v);
}

Component GradientSlider(float* value, float min, float max, std::string label,
                         std::function<Utils::Colors::RGB(float)> colorAt,
                         std::function<std::string(float)> format,
                         std::function<void()> onChange, int width, int height,
                         Orientation orientation) {
  return Make<GradientSliderBase>(value, min, max, std::move(label),
                                  std::move(colorAt), std::move(format),
                                  std::move(onChange), width, height,
                                  orientation);
}
