#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <vector>
#include <memory>

#include <app.hpp>
#include <file_extractor.hpp>
#include <cxxopts.hpp>
#include <safe_arg.hpp>

class MessageWindow final : public ftxui::ComponentBase {
public:
  ftxui::Element Render() override {
    using namespace ftxui;
    return get_formatted_message();
  }

  bool
  OnEvent(ftxui::Event event) override {
    // Release lock if locking the screen and the event match
    if (m_required_response && m_required_response == event) {
      clear();
      return true;
    }

    if (ComponentBase::OnEvent(event)) {
      return true;
    }

    return false;
  };

  void OnAnimation(ftxui::animation::Params& params) override {
    // Do nothing
    (void)params;
  }

  bool Focusable() const override { return true; }

  void warning(std::string&& message) {
    m_displayed_message = Message {message, MessageType::WARNING};
    m_required_response = ftxui::Event::Escape;
  }

  void info(std::string&& message) { m_displayed_message = Message {message, MessageType::INFO}; }

  void error(std::string&& message) {
    m_displayed_message = Message {message, MessageType::ERROR};

    m_required_response = ftxui::Event::Escape;
  }

  void clear() {
    m_displayed_message.reset();
    m_required_response.reset();
  }

  bool is_locking_screen() { return m_required_response.has_value(); }

private:
  enum class MessageType { INFO, WARNING, ERROR };
  struct Message {
    std::string content;
    MessageType type;
  };

  std::optional<Message> m_displayed_message;

  std::optional<ftxui::Event> m_required_response;

  ftxui::Element get_formatted_message() {
    using namespace ftxui;
    if (!m_displayed_message) {
      return text("");
    }

    if (m_displayed_message->type == MessageType::ERROR) {
      return paragraph("Error: " + m_displayed_message->content + ". Press Escape to continue.")
        | color(Color::Red);
    }

    if (m_displayed_message->type == MessageType::WARNING) {
      return paragraph("Warning: " + m_displayed_message->content + ". Press Escape to continue.")
        | color(Color::Yellow);
    }

    // Info
    return paragraph(m_displayed_message->content) | color(Color::Blue);
  }


};

class CommandWindow final : public ftxui::ComponentBase {
public:
  CommandWindow(std::function<void(std::string)> execute): m_execute(execute) {
    init_input_field();
  }

  bool OnEvent(ftxui::Event event) override { return m_input->OnEvent(event); }

  ftxui::Element Render() override {
    using namespace ftxui;
    return hbox({text("/"), m_input->Render()})
           | size(WidthOrHeight::HEIGHT, Constraint::GREATER_THAN, 2);
  }

  void
  OnAnimation(ftxui::animation::Params& params) override {
    // Do nothing
    (void)params;
  }

  bool Focusable() const override { return true; }

  void
  clear() {
    m_current_command.clear();
    init_input_field();
  }

private:
  std::string m_current_command;
  ftxui::Component m_input;
  std::function<void(std::string)> m_execute;

  void
  init_input_field() {
    auto input_option = ftxui::InputOption();
    input_option.multiline = false;
    input_option.on_enter = [this] {
      m_execute(m_current_command);

      // After execution, clear the current command
      clear();
    };
    m_input = ftxui::Input(&m_current_command, input_option);
  }
};

class EditWindow final : public ftxui::ComponentBase {
public:
  EditWindow(std::shared_ptr<EditWindowExtractor> extractor):
    m_extractor(extractor),
    m_last_screen_dim_x(0),
    m_last_screen_dim_y(0) {


  }

  ftxui::Element Render() override {
    using namespace ftxui;

    // Adjust size if needed
    adjust_size();

    std::vector<ftxui::Element> line_texts;
    for (const std::string& line: m_extractor->get_lines()) {
      line_texts.emplace_back(ftxui::text(line));
    }

    std::string formatted_fsize = std::to_string(m_extractor->get_size()) + " bytes";

    std::string formatted_pos = std::to_string(m_extractor->get_streampos()) + " bytes";

    auto element = window(text(m_extractor->get_fpath() + " [" + formatted_pos + " / "
      + formatted_fsize + "]")
      | color(Color::Blue) | blink,
      vbox(line_texts));

    element = flex_grow(element);
    element |= ftxui::reflect(m_box);

    return element;
  }

  bool
  OnEvent(ftxui::Event event) override {
    using namespace ftxui;

    // Adjust size if needed
    adjust_size();

    if (event == Event::ArrowDown ||
      (event.mouse().button == Mouse::Button::WheelDown)) {
      if (m_extractor->can_move_down()) {
        m_extractor->move_down();
      }
      return true;
    }

    if (event == Event::ArrowUp ||
      (event.mouse().button == Mouse::Button::WheelUp)) {
      if (m_extractor->can_move_up()) {
        m_extractor->move_up();
      }
      return true;
    }

    return false;
  }

  void
  OnAnimation(ftxui::animation::Params& params) override {
    // Do nothing
    (void)params;
  }

  bool Focusable() const override { return true; }
private:
  std::shared_ptr<EditWindowExtractor> m_extractor;
  int m_last_screen_dim_x;
  int m_last_screen_dim_y;

  ftxui::Box m_box;

  void adjust_size() {
    int dimx = m_box.x_max - m_box.x_min + 1;
    int dimy = m_box.y_max - m_box.y_min + 1;

    if (dimx != m_last_screen_dim_x || dimy != m_last_screen_dim_y) {
      // Resize the screen
      m_last_screen_dim_x = dimx;
      m_last_screen_dim_y = dimy;
      m_extractor->set_size(std::max(1, dimx - 2), std::max(1, dimy - 2));
    }
  }
};

class FileEditor : public ftxui::ComponentBase {
public:
  FileEditor(std::shared_ptr<EditWindow> edit_window, std::shared_ptr<EditWindowExtractor> extractor):
    m_mode(Mode::VIEW),

    m_edit_window(edit_window),
    m_command_window(std::make_shared<CommandWindow>([this](std::string command) { execute_command(command); })),
    m_message_window(std::make_shared<MessageWindow>()),

    m_extractor(extractor),

    m_jump_options("jump", "Jump to a location if the file"),
    m_search_options("search", "Search a pattern")
  {
    m_jump_options.add_options()
      ("p,position", "Position to jump to in bytes", cxxopts::value<int64_t>());
    m_jump_options.parse_positional({"position"});

    m_search_options.add_options()
      ("p,pattern", "Pattern to search")
      ("f,from", "Starting position in bytes", cxxopts::value<int64_t>()->default_value("0"))
      ("t,to", "Ending position in bytes", cxxopts::value<int64_t>()->default_value("0"));
    m_search_options.parse_positional({"pattern"});
  }

  bool OnEvent(ftxui::Event event) override {
    using namespace ftxui;
    // Handle special events
    if (event == Event::Special({0})) {
      m_edit_window->OnEvent(event);
      m_message_window->OnEvent(event);
      m_command_window->OnEvent(event);
      return true;
    }

    // Handle unspecial events
    if (m_message_window->is_locking_screen()) {
      return m_message_window->OnEvent(event);
    }

    if (event == Event::Escape) {
      if (m_mode != Mode::VIEW) {
        switch_mode(Mode::VIEW);
        return true;
      }
    } else if (event == Event::Character('/')) {
      if (m_mode != Mode::COMMAND) {
        switch_mode(Mode::COMMAND);
        return true;
      }
    }

    if (m_mode == Mode::VIEW) {
      return m_edit_window->OnEvent(event);
    }
    if (m_mode == Mode::COMMAND) {
      return m_command_window->OnEvent(event);
    }

    // Should not happen
    exit(-1);

    return false;
  }

  ftxui::Element Render() override {
    if (m_mode == Mode::VIEW) {
      // View mode
      return ftxui::vbox({m_edit_window->Render()});
    } else {
      // Command mode
      return ftxui::vbox({
        m_edit_window->Render(),
        m_message_window->Render(),
        m_command_window->Render(),
      });
    }
  }

  void OnAnimation(ftxui::animation::Params& params) override {
    // Do nothing
    (void)params;
  }

  bool Focusable() const override { return true; }

private:
  Mode m_mode;
  std::shared_ptr<EditWindow> m_edit_window;
  std::shared_ptr<CommandWindow> m_command_window;
  std::shared_ptr<MessageWindow> m_message_window;
  std::shared_ptr<EditWindowExtractor> m_extractor;

  // Users' commands parsers
  cxxopts::Options m_jump_options;
  cxxopts::Options m_search_options;

  void switch_mode(Mode new_mode) {
    clear_current_mode();
    set_mode(new_mode);
  }

  void clear_current_mode() {
    if (m_mode == Mode::COMMAND) {
      m_command_window->clear();
    }
    m_message_window->clear();
  }

  void set_mode(Mode mode) { m_mode = mode; }

  // static constexpr std::string
  void execute_command(std::string command) {
    SafeArg safe_arg(command);
    std::string command_type(safe_arg.get_argv()[0]);

    if (command_type == "exit") {
      exit(0);
    } else if (command_type == "jump") {
      int64_t pos = -1;

      try {
        cxxopts::ParseResult parse_result = m_jump_options.parse(safe_arg.get_argc(), safe_arg.get_argv());
        pos = parse_result["position"].as<int64_t>();
      } catch (cxxopts::OptionException e) {
        m_message_window->error(e.what());
        return;
      }

      if (pos < 0) {
        m_message_window->error("Invalid position: " + std::to_string(pos) + " < 0.");
      } else if (pos >= m_extractor->get_size()) {
        m_message_window->error("Position " + std::to_string(pos) + " exceeded file size.");
      } else {
        m_extractor->move_to(pos);
      }
    } else {
      m_message_window->error("No such command: " + command);
    }
  }
};

void run_app(std::string fpath) {
  using namespace ftxui;

  auto extractor = std::make_shared<EditWindowExtractor>(fpath);

  auto edit_window = std::make_shared<EditWindow>(extractor);

  auto file_editor = std::make_shared<FileEditor>(edit_window, extractor);

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  try {
    screen.Loop(file_editor);
  } catch (std::string const & e) {
    std::cout << e;
  }
}