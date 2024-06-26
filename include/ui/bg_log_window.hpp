#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>

class BgLogWindow final : public ftxui::ComponentBase {
public:
  ftxui::Element Render() override {
    return ftxui::paragraph(_message) | ftxui::color(ftxui::Color::SkyBlue1) | ftxui::bold;
  }

  bool OnEvent(ftxui::Event /*unused*/) override {
    // Do nothing.
    return false;
  }

void OnAnimation(ftxui::animation::Params& /*unused*/) override {
// Do nothing
}

  bool Focusable() const override { return true; }

  // APIs for state mutation by parent components and
  // updaters/synchronisers
  // Not thread-safe. Requires updaters/synchronisers to run on the same thread
  void setMessage(std::string message) { _message = message; }

  void clear() { _message.clear(); }

private:
  std::string _message;
};