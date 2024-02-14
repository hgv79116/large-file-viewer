#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <vector>
#include <memory>

#include <app.hpp>
#include <file_extractor.hpp>
#include <cxxopts.hpp>
#include <safe_arg.hpp>
#include <background_task_runner.hpp>
#include <search_stream.hpp>

constexpr int32_t DEFAULT_MATCH_LIMIT = 5e6;

// The components below follow a React-ive pattern by gathering all 3 concerns in one component:
// - Rendering
// - Updating state based on users' input
// - Synchronising with background task
// This pattern somewhat helps isolate data based on the UI's separation, but entangle different kinds
// logic in one place

class BackgroundTaskMessageWindow final : public ftxui::ComponentBase {
public:
  ftxui::Element Render() override {
    return ftxui::paragraph(m_message) | ftxui::color(ftxui::Color::SkyBlue1) | ftxui::bold;
  }

  bool
  OnEvent(ftxui::Event event) override {
    (void)event;
    // Do nothing.
    return false;
  };

  void
  OnAnimation(ftxui::animation::Params& params) override {
    // Do nothing
    (void)params;
  }

  bool Focusable() const override { return true; }

  // APIs for state mutation by parent components and
  // updaters/synchronisers
  // Not thread-safe. Requires updaters/synchronisers to run on the same thread
  void set_message(std::string&& message) { m_message = message; }

  void clear() { m_message.clear(); }

private:
  std::string m_message;
};

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
        | color(Color::RedLight) | bold;
    }

    if (m_displayed_message->type == MessageType::WARNING) {
      return paragraph("Warning: " + m_displayed_message->content + ". Press Escape to continue.")
        | color(Color::Yellow) | bold;
    }

    // Info
    return paragraph(m_displayed_message->content) | color(Color::LightCoral) | bold;
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
      | color(Color::GreenLight) | bold,
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
  std::shared_ptr<BackgroundTaskMessageWindow> m_task_message_window;
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
  FileEditor(
    std::shared_ptr<EditWindow> edit_window,
    std::shared_ptr<EditWindowExtractor> extractor,
    std::shared_ptr<BackgroundTaskMessageWindow> task_message_window):
    m_mode(Mode::VIEW),

    m_edit_window(edit_window),
    m_task_message_window(task_message_window),
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
      ("p,pattern", "Pattern to search", cxxopts::value<std::string>())
      ("f,from", "Starting position in bytes", cxxopts::value<int64_t>()->default_value("0"))
      ("t,to", "Ending position in bytes",
        cxxopts::value<int64_t>()->default_value(std::to_string(m_extractor->get_size())));
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

    // Handle search events, if searching
    if (m_search_result != nullptr) {
      int num_match = m_search_result->get_num_matches();
      BackgroundTaskStatus status = m_search_result->status;
      bool search_finished_or_aborted
          = status == BackgroundTaskStatus::FINISHED || status == BackgroundTaskStatus::ABORTED;
      if (event == Event::Tab) {
        // User wants to find the next match
        if (num_match == 0) {
          m_message_window->error("No matches found yet");
        } else if (m_displayed_search_index + 1 >= num_match) {
          if (!search_finished_or_aborted) {
            m_message_window->error("Next match not found yet");
          }
          else {
            m_message_window->error("No more matches found");
          }
        } else {
          m_displayed_search_index++;
          m_extractor->move_to(m_search_result->get_match(m_displayed_search_index));
        }
      } else if (event == Event::TabReverse) {
        // User wants to find previous match
        if (num_match == 0) {
          m_message_window->error("No matches found yet");
        } else if (m_displayed_search_index - 1 < 0) {
          m_message_window->error("No previous matches");
        } else {
          m_displayed_search_index--;
          m_extractor->move_to(m_search_result->get_match(m_displayed_search_index));
        }
      }
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
    // Synchronise UI state with background task if needed
    if (m_search_result != nullptr) {
      BackgroundTaskStatus status = m_search_result->status;
      switch (status) {
        case BackgroundTaskStatus::NOT_STARTED:
          m_task_message_window->set_message("Search pending");
          break;
        case BackgroundTaskStatus::ONGOING:
          m_task_message_window->set_message(
              "Searched until location " + std::to_string(m_search_result->get_current_pos()) + "... "
              + std::to_string(m_search_result->get_num_matches()) + " occurences found.");
          break;
        case BackgroundTaskStatus::FINISHED:
          m_task_message_window->set_message(
              "Searched completed. "
              + std::to_string(m_search_result->get_num_matches()) + " occurences found.");
          break;
        case BackgroundTaskStatus::ABORTED:
          m_task_message_window->set_message("Search canceled!");
          break;
      }
    }


    if (m_mode == Mode::VIEW) {
      // View mode
      return ftxui::vbox({
        m_edit_window->Render(),
        m_task_message_window->Render()
      });
    } else {
      // Command mode
      return ftxui::vbox({
        m_edit_window->Render(),
        m_task_message_window->Render(),
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
  std::shared_ptr<BackgroundTaskMessageWindow> m_task_message_window;
  std::shared_ptr<CommandWindow> m_command_window;
  std::shared_ptr<MessageWindow> m_message_window;
  std::shared_ptr<EditWindowExtractor> m_extractor;

  // Users' commands parsers
  cxxopts::Options m_jump_options;
  cxxopts::Options m_search_options;

  // Search result, if there is any
  static constexpr int NOT_DISPLAYED = -1;
  int m_displayed_search_index;
  std::shared_ptr<SearchResult> m_search_result;
  std::shared_ptr<std::atomic<bool>> m_search_aborted;

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

    try {
      if (command_type == "exit") {
        exit(0);
      } else if (command_type == "jump") {
        cxxopts::ParseResult parse_result = m_jump_options.parse(safe_arg.get_argc(), safe_arg.get_argv());
        int64_t pos = parse_result["position"].as<int64_t>();

        if (pos < 0 || pos >= m_extractor->get_size()) {
          m_message_window->error("Invalid position: " + std::to_string(pos));
        } else {
          m_extractor->move_to(pos);
        }
      } else if (command_type == "search") {
        cxxopts::ParseResult parse_result
            = m_search_options.parse(safe_arg.get_argc(), safe_arg.get_argv());

        std::string pattern = parse_result["pattern"].as<std::string>();
        int64_t from = parse_result["from"].as<int64_t>();
        int64_t to = parse_result["to"].as<int64_t>();

        if (pattern.empty()) {
          m_message_window->error("Pattern cannot be empty");
        } else if (from > to || from < 0 || from > m_extractor->get_size() ||
          to < 0 || to > m_extractor->get_size()) {
          m_message_window->error("Invalid range: " + std::to_string(from) + " - " + std::to_string(to));
        } else {
          BackgroundTaskRunner& runner = BackgroundTaskRunner::get_instance();

          if (!runner.can_run_task()) {
            m_message_window->error("Already running a background task. ");
          }
          else {
            std::cerr << "Almost done" << std::endl;

            // Reset search variables
            reset_search();

            std::cerr << "REset done" << std::endl;

            // Capturing by reference leads to reading trash values when
            // the lambda is called later.
            runner.run_task([this, pattern, from, to] {
              search_in_stream(std::ifstream(m_extractor->get_fpath()),
                               pattern, from, to, DEFAULT_MATCH_LIMIT, m_search_result, m_search_aborted);
            });
          };
        }
      } else if (command_type == "cancel") {
        // Request search to cancel. The thread running this search won't really be stopped until
        // it reads the signal.
        *m_search_aborted = true;
      } else {
        m_message_window->error("No such command: " + command);
      }
    } catch (cxxopts::OptionException e) {
      m_message_window->error(e.what());
      return;
    }
  }

  void reset_search() {
    // Reset search variables
    m_displayed_search_index = -1;
    m_search_result = std::make_shared<SearchResult>();
    m_search_aborted = std::make_shared<std::atomic<bool>>(false);
    std::cerr << "here" << std::endl;
  }
};

void run_app(std::string fpath) {
  using namespace ftxui;

  auto extractor = std::make_shared<EditWindowExtractor>(fpath);

  auto background_task_message_window = std::make_shared<BackgroundTaskMessageWindow>();

  auto edit_window = std::make_shared<EditWindow>(extractor);

  auto file_editor = std::make_shared<FileEditor>(edit_window, extractor, background_task_message_window);

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  screen.Loop(file_editor);
}