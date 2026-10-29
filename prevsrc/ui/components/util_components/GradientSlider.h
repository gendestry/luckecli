#pragma once
// A slider drawn on an FTXUI Canvas with a custom gradient fill. The track is
// painted pixel by pixel using a user supplied colour function, a draggable
// handle shows the current value, and a label sits alongside.
//
// Vertical:   min at the bottom, max at the top; value text drawn on the thumb.
// Horizontal: min at the left,   max at the right; value text shown beside the
//             track (the thumb is too thin for text).

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <Utils/Colors/Colors.h>

#include <functional>
#include <string>

enum class Orientation { Vertical, Horizontal };

class GradientSliderBase : public ftxui::ComponentBase {
public:
  // colorAt: value -> RGB, used to paint the gradient (called per pixel line).
  // format:  value -> the text label (e.g. "63" -> "63°").
  // width/height are in canvas pixels: 2 px per terminal column, 4 px per row.
  // For a clean grid use a multiple of 2 for width and a multiple of 4 for
  // height.
  // An empty colorAt gives a plain (uncoloured) slider; an empty format shows
  // the integer value.
  GradientSliderBase(float* value, float min, float max, std::string label,
                     std::function<Utils::Colors::RGB(float)> colorAt = {},
                     std::function<std::string(float)> format = {},
                     std::function<void()> onChange = {}, int width = 24,
                     int height = 100,
                     Orientation orientation = Orientation::Vertical);

  ftxui::Element OnRender() override;
  bool OnEvent(ftxui::Event event) override;
  bool Focusable() const final { return true; }

private:
  bool vertical() const { return orientation_ == Orientation::Vertical; }
  // Length of the axis the value slides along (pixels).
  int axisLen() const { return vertical() ? kH : kW; }
  int valueToPixel(float v) const;   // position along the value axis
  float pixelToValue(int pos) const; // inverse

  int kW; // pixels wide  (2 px per cell)
  int kH; // pixels tall  (4 px per cell)

  float* value_;
  float min_, max_;
  std::string label_;
  std::function<Utils::Colors::RGB(float)> colorAt_;
  std::function<std::string(float)> format_;
  std::function<void()> onChange_;
  Orientation orientation_;
  bool captured_ = false;
  ftxui::Box box_;
};

ftxui::Component
GradientSlider(float* value, float min, float max, std::string label,
               std::function<Utils::Colors::RGB(float)> colorAt = {},
               std::function<std::string(float)> format = {},
               std::function<void()> onChange = {}, int width = 24,
               int height = 100,
               Orientation orientation = Orientation::Vertical);
