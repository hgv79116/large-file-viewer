#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>

class CommandWindow final : public ftxui::ComponentBase {
public:
  CommandWindow(std::function<void(std::string)> execute) : _execute(std::move(execute)) {
    initInputField();
  }

  bool OnEvent(ftxui::Event event) override { return _input->OnEvent(event); }

  ftxui::Element Render() override {
    using namespace ftxui;
    return hbox({text("/"), _input->Render()})
           | size(WidthOrHeight::HEIGHT, Constraint::GREATER_THAN, 2);
  }

  void OnAnimation(ftxui::animation::Params& /* unused */) override {
    // Do nothing
  }

  bool Focusable() const override { return true; }

  void clear() {
    _input->Detach();

    _current_command.clear();

    initInputField();
  }

private:
  std::string _current_command;
  ftxui::Component _input;
  std::function<void(std::string)> _execute;

  void initInputField() {
    auto input_option = ftxui::InputOption();
    input_option.multiline = false;
    input_option.on_enter = [this] {
      _execute(_current_command);

      // After execution, clear the current command
      clear();
    };
    _input = ftxui::Input(&_current_command, input_option);
    Add(_input);
  }
};