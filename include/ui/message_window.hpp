#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>

#include <string>
#include <optional>

class MessageWindow final : public ftxui::ComponentBase {
public:
  ftxui::Element Render() override {
    using namespace ftxui;
    return getFormattedMessage();
  }

  bool OnEvent(ftxui::Event event) override;

  void OnAnimation(ftxui::animation::Params& /*unused*/) override {
    // Do nothing
  }

  bool Focusable() const override { return true; }

  void warning(std::string message) {
    _displayed_message = Message{message, MessageType::WARNING};
    _required_response = ftxui::Event::Escape;
  }

  void info(std::string message) { _displayed_message = Message{message, MessageType::INFO}; }

  void error(std::string message) {
    _displayed_message = Message{message, MessageType::ERROR};

    _required_response = ftxui::Event::Escape;
  }

  void clear() {
    _displayed_message.reset();
    _required_response.reset();
  }

  bool isLockingScreen() { return _required_response.has_value(); }

private:
  enum class MessageType { INFO, WARNING, ERROR };
  struct Message {
    std::string content;
    MessageType type;
  };

  std::optional<Message> _displayed_message;

  std::optional<ftxui::Event> _required_response;

  ftxui::Element getFormattedMessage();
};