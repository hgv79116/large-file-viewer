#include "edit_window_extractor.hpp"
#include "bg_log_window.hpp"
#include "util/file_metadata.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>

#include <memory>

class EditWindow final : public ftxui::ComponentBase {
public:
  EditWindow(std::shared_ptr<EditWindowExtractor> extractor, FileMetadata fmeta)
    : _extractor{extractor},
      _fmeta{fmeta}
    {}

  ftxui::Element Render() override {
    using namespace ftxui;

    // Adjust size if needed
    adjustSize();

    std::vector<ftxui::Element> line_texts;
    for (const std::string& line : _extractor->get_lines()) {
      line_texts.emplace_back(ftxui::text(line));
    }

    std::string formatted_fsize = std::to_string(_fmeta.getSize()) + " bytes";

    std::string formatted_pos = std::to_string(_extractor->get_streampos()) + " bytes";

    auto element = window(
        text(_fmeta.getPath() + " [" + formatted_pos + " / " + formatted_fsize + "]")
            | color(Color::GreenLight) | bold,
        vbox(line_texts));

    element = flex_grow(element);
    element |= ftxui::reflect(_box);

    return element;
  }

  bool OnEvent(ftxui::Event event) override {
    using namespace ftxui;

    // Adjust size if needed
    adjustSize();

    if (event == Event::ArrowDown || (event.mouse().button == Mouse::Button::WheelDown)) {
      if (_extractor->can_move_down()) {
        _extractor->move_down();
      }
      return true;
    }

    if (event == Event::ArrowUp || (event.mouse().button == Mouse::Button::WheelUp)) {
      if (_extractor->can_move_up()) {
        _extractor->move_up();
      }
      return true;
    }

    return false;
  }

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  bool Focusable() const override { return true; }

private:
  std::shared_ptr<EditWindowExtractor> _extractor;
  std::shared_ptr<BgLogWindow> _task_message_window;
  FileMetadata _fmeta;
  int _last_di_x = 0;
  int _last_di_y = 0;

  ftxui::Box _box;

  void adjustSize() {
    int dimx = _box.x_max - _box.x_min + 1;
    int dimy = _box.y_max - _box.y_min + 1;

    if (dimx != _last_di_x || dimy != _last_di_y) {
      // Resize the screen
      _last_di_x = dimx;
      _last_di_y = dimy;
      _extractor->set_size(std::max(1, dimx - 2), std::max(1, dimy - 2));
    }
  }
};