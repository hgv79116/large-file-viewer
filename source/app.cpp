#include <LFV/app.hpp>
#include <LFV/background_task_runner.hpp>
#include <LFV/file_extractor.hpp>
#include <LFV/safe_arg.hpp>
#include <LFV/search_stream.hpp>
#include <cstdint>
#include <cxxopts.hpp>
#include <deque>>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <memory>
#include <string>
#include <vector>

constexpr int32_t DEFAULT_MATCH_LIMIT = 5'000'000;
constexpr int32_t DEFAULT_SYNCHRONISATION_DELAY = 30;

// The components below follow a React-ive pattern by gathering all 3 concerns in one component:
// - Rendering
// - Updating state based on users' input
// - Synchronising with background task
// This pattern somewhat helps isolate data based on the UI's separation, but entangle different
// kinds logic in one place

class BackgroundTaskMessageWindow final : public ftxui::ComponentBase {
public:
  auto Render() -> ftxui::Element override {
    return ftxui::paragraph(m_message) | ftxui::color(ftxui::Color::SkyBlue1) | ftxui::bold;
  }

  auto OnEvent([[maybe_unused]] ftxui::Event event) -> bool override {
    // Do nothing.
    return false;
  };

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  [[nodiscard]] auto Focusable() const -> bool override { return true; }

  // APIs for state mutation by parent components and
  // updaters/synchronisers
  // Not thread-safe. Requires updaters/synchronisers to run on the same thread
  void set_message(std::string message) { m_message = message; }

  void clear() { m_message.clear(); }

private:
  std::string m_message;
};

class MessageWindow final : public ftxui::ComponentBase {
public:
  auto Render() -> ftxui::Element override {
    using namespace ftxui;
    return get_formatted_message();
  }

  auto OnEvent(ftxui::Event event) -> bool override {
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

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  [[nodiscard]] auto Focusable() const -> bool override { return true; }

  void warning(std::string message) {
    m_displayed_message = Message{message, MessageType::WARNING};
    m_required_response = ftxui::Event::Escape;
  }

  void info(std::string message) { m_displayed_message = Message{message, MessageType::INFO}; }

  void error(std::string message) {
    m_displayed_message = Message{message, MessageType::ERROR};

    m_required_response = ftxui::Event::Escape;
  }

  void clear() {
    m_displayed_message.reset();
    m_required_response.reset();
  }

  auto is_locking_screen() -> bool { return m_required_response.has_value(); }

private:
  enum class MessageType { INFO, WARNING, ERROR };
  struct Message {
    std::string content;
    MessageType type;
  };

  std::optional<Message> m_displayed_message;

  std::optional<ftxui::Event> m_required_response = {};

  auto get_formatted_message() -> ftxui::Element {
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
  CommandWindow(std::function<void(std::string)> execute) : m_execute(std::move(execute)) {
    init_input_field();
  }

  auto OnEvent(ftxui::Event event) -> bool override { return m_input->OnEvent(event); }

  auto Render() -> ftxui::Element override {
    using namespace ftxui;
    return hbox({text("/"), m_input->Render()})
           | size(WidthOrHeight::HEIGHT, Constraint::GREATER_THAN, 2);
  }

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  [[nodiscard]] auto Focusable() const -> bool override { return true; }

  void clear() {
    m_input->Detach();

    m_current_command.clear();

    init_input_field();
  }

private:
  std::string m_current_command;
  ftxui::Component m_input;
  std::function<void(std::string)> m_execute;

  void init_input_field() {
    auto input_option = ftxui::InputOption();
    input_option.multiline = false;
    input_option.on_enter = [this] {
      m_execute(m_current_command);

      // After execution, clear the current command
      clear();
    };
    m_input = ftxui::Input(&m_current_command, input_option);
    Add(m_input);
  }
};

class EditWindow final : public ftxui::ComponentBase {
public:
  EditWindow(std::shared_ptr<EditWindowExtractor> extractor) : m_extractor(std::move(extractor)) {}

  auto Render() -> ftxui::Element override {
    using namespace ftxui;

    // Adjust size if needed
    adjust_size();

    std::vector<ftxui::Element> line_texts;
    for (const std::string& line : m_extractor->get_lines()) {
      line_texts.emplace_back(ftxui::text(line));
    }

    std::string formatted_fsize = std::to_string(m_extractor->get_size()) + " bytes";

    std::string formatted_pos = std::to_string(m_extractor->get_streampos()) + " bytes";

    auto element = window(
        text(m_extractor->get_fpath() + " [" + formatted_pos + " / " + formatted_fsize + "]")
            | color(Color::GreenLight) | bold,
        vbox(line_texts));

    element = flex_grow(element);
    element |= ftxui::reflect(m_box);

    return element;
  }

  auto OnEvent(ftxui::Event event) -> bool override {
    using namespace ftxui;

    // Adjust size if needed
    adjust_size();

    if (event == Event::ArrowDown || (event.mouse().button == Mouse::Button::WheelDown)) {
      if (m_extractor->can_move_down()) {
        m_extractor->move_down();
      }
      return true;
    }

    if (event == Event::ArrowUp || (event.mouse().button == Mouse::Button::WheelUp)) {
      if (m_extractor->can_move_up()) {
        m_extractor->move_up();
      }
      return true;
    }

    return false;
  }

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  [[nodiscard]] auto Focusable() const -> bool override { return true; }

private:
  std::shared_ptr<EditWindowExtractor> m_extractor;
  std::shared_ptr<BackgroundTaskMessageWindow> m_task_message_window;
  int m_last_dim_x = 0;
  int m_last_dim_y = 0;

  ftxui::Box m_box;

  void adjust_size() {
    int dimx = m_box.x_max - m_box.x_min + 1;
    int dimy = m_box.y_max - m_box.y_min + 1;

    if (dimx != m_last_dim_x || dimy != m_last_dim_y) {
      // Resize the screen
      m_last_dim_x = dimx;
      m_last_dim_y = dimy;
      m_extractor->set_size(std::max(1, dimx - 2), std::max(1, dimy - 2));
    }
  }
};

class FileEditor : public ftxui::ComponentBase {
public:
  FileEditor(std::shared_ptr<EditWindow> edit_window,
             std::shared_ptr<EditWindowExtractor> extractor,
             std::shared_ptr<BackgroundTaskMessageWindow> task_message_window,
             std::shared_ptr<BackgroundTaskRunner> runner_ptr)
      : m_edit_window(std::move(edit_window)),
        m_task_message_window(std::move(task_message_window)),
        m_command_window(std::make_shared<CommandWindow>(
            [this](std::string command) { execute_command(command); })),
        m_message_window(std::make_shared<MessageWindow>()),

        m_extractor(std::move(extractor)),

        m_runner_ptr(std::move(runner_ptr)),

        m_jump_options("jump", "Jump to a location if the file"),
        m_search_options("search", "Search a pattern") {
    m_jump_options.add_options()("p,position", "Position to jump to in bytes",
                                 cxxopts::value<long long>());
    m_jump_options.parse_positional({"position"});

    m_search_options.add_options()("p,pattern", "Pattern to search", cxxopts::value<std::string>())(
        "f,from", "Starting position in bytes", cxxopts::value<long long>()->default_value("0"))(
        "t,to", "Ending position in bytes",
        cxxopts::value<long long>()->default_value(std::to_string(m_extractor->get_size())));
    m_search_options.parse_positional({"pattern"});

    Add(m_edit_window);
    Add(m_task_message_window);
    Add(m_message_window);
    Add(m_command_window);
  }

  auto OnEvent(ftxui::Event event) -> bool override {
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
      if (handleSearchEvents(event)) {
        return true;
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

  auto Render() -> ftxui::Element override {
    if (m_mode == Mode::VIEW) {
      // View mode
      return ftxui::vbox({m_edit_window->Render(), m_task_message_window->Render()});
    }

    // Command mode
    return ftxui::vbox({
        m_edit_window->Render(),
        m_task_message_window->Render(),
        m_message_window->Render(),
        m_command_window->Render(),
    });
  }

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  [[nodiscard]] auto Focusable() const -> bool override { return true; }

  void synchronise() {
    // Synchronise UI state with background task if needed
    if (m_search_result != nullptr) {
      BackgroundTaskStatus status = m_search_result->get_status();
      switch (status) {
        case BackgroundTaskStatus::NOT_STARTED:
          m_task_message_window->set_message("Search pending");
          break;
        case BackgroundTaskStatus::ONGOING:
          m_task_message_window->set_message(
              "Searched until location " + std::to_string(m_search_result->get_current_pos())
              + "... " + std::to_string(m_search_result->get_num_matches()) + " occurences found.");
          break;
        case BackgroundTaskStatus::FINISHED:
          m_task_message_window->set_message("Searche completed. "
                                             + std::to_string(m_search_result->get_num_matches())
                                             + " occurences found.");
          break;
        case BackgroundTaskStatus::ABORTED:
          m_task_message_window->set_message("Search canceled!");
          break;
      }
    }
  }

private:
  Mode m_mode = Mode::VIEW;
  std::shared_ptr<EditWindow> m_edit_window;
  std::shared_ptr<BackgroundTaskMessageWindow> m_task_message_window;
  std::shared_ptr<CommandWindow> m_command_window;
  std::shared_ptr<MessageWindow> m_message_window;
  std::shared_ptr<EditWindowExtractor> m_extractor;
  std::shared_ptr<BackgroundTaskRunner> m_runner_ptr;

  // Users' commands parsers
  cxxopts::Options m_jump_options;
  cxxopts::Options m_search_options;

  // Search result, if there is any
  static constexpr int NOT_DISPLAYED = -1;
  int m_displayed_search_index = NOT_DISPLAYED;
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
    try {
      execute_command_unguarded(std::move(command));
    } catch (cxxopts::OptionException const& e) {
      m_message_window->error(e.what());
      return;
    }
  }

  void execute_command_unguarded(const std::string& command) {
    SafeArg safe_arg(command);
    std::string command_type(safe_arg.get_argv()[0]);

    if (command_type == "exit") {
      exit(0);
    }

    if (command_type == "jump") {
      execute_jump_command(safe_arg);
      return;
    }

    if (command_type == "search") {
      execute_search_command(safe_arg);
      return;
    }

    if (command_type == "cancel") {
      // Request search to cancel. The thread running this search won't really be stopped until
      // it reads the signal.
      *m_search_aborted = true;
      return;
    }

    m_message_window->error("No such command: " + command);
  }

  void execute_jump_command(const SafeArg& safe_arg) {
    cxxopts::ParseResult parse_result
        = m_jump_options.parse(safe_arg.get_argc(), safe_arg.get_argv());

    auto pos = static_cast<std::streampos>(parse_result["position"].as<long long>());

    if (pos < 0 || pos >= m_extractor->get_end()) {
      m_message_window->error("Invalid position: " + std::to_string(pos));
    } else {
      m_extractor->move_to(pos);
      m_message_window->info("Jumped to " + std::to_string(pos));
    }
  }

  void execute_search_command(const SafeArg& safe_arg) {
    cxxopts::ParseResult parse_result
        = m_search_options.parse(safe_arg.get_argc(), safe_arg.get_argv());

    auto pattern = parse_result["pattern"].as<std::string>();
    auto from = static_cast<std::streampos>(parse_result["from"].as<long long>());
    auto to = static_cast<std::streampos>(parse_result["to"].as<long long>());

    if (pattern.empty()) {
      m_message_window->error("Pattern cannot be empty");
      return;
    }

    if (from > to || from < 0 || from > m_extractor->get_end() || to < 0
        || to > m_extractor->get_end()) {
      m_message_window->error("Invalid range: " + std::to_string(from) + " - "
                              + std::to_string(to));
      return;
    }

    if (!m_runner_ptr->can_run_task()) {
      m_message_window->error("Already running a background task. ");
      return;
    }

    // Reset search variables
    reset_search();

    // Capturing by reference leads to reading trash values when
    // the lambda is called later.
    m_runner_ptr->run_task([this, pattern, from, to] {
      search_in_stream(std::ifstream(m_extractor->get_fpath()), pattern, from, to,
                       DEFAULT_MATCH_LIMIT, m_search_result, m_search_aborted);
    });
  }

  void reset_search() {
    // Reset search variables
    m_displayed_search_index = NOT_DISPLAYED;
    m_search_result = std::make_shared<SearchResult>();
    m_search_aborted = std::make_shared<std::atomic<bool>>(false);
  }

  auto handleSearchEvents(ftxui::Event event) -> bool {
    using namespace ftxui;

    if (event == Event::Tab) {
      // User wants to find the next match
      if (handleSearchTabEvent()) {
        return true;
      }
    }

    if (event == Event::TabReverse) {
      // User wants to find previous match
      if (handleSearchReverseTabEvent()) {
        return true;
      }
    }

    return false;
  }

  auto handleSearchTabEvent() -> bool {
    int num_match = m_search_result->get_num_matches();
    BackgroundTaskStatus status = m_search_result->get_status();
    bool search_finished_or_aborted
        = status == BackgroundTaskStatus::FINISHED || status == BackgroundTaskStatus::ABORTED;

    if (num_match == 0) {
      m_message_window->error("No matches found yet");
      return true;
    }

    if (m_displayed_search_index + 1 >= num_match) {
      if (!search_finished_or_aborted) {
        m_message_window->error("Next match not found yet");
        return true;
      }

      m_message_window->error("No more matches found");
      return true;
    }

    m_displayed_search_index++;
    m_extractor->move_to(m_search_result->get_match(m_displayed_search_index));
    return true;
  }

  auto handleSearchReverseTabEvent() -> bool {
    int num_match = m_search_result->get_num_matches();

    if (num_match == 0) {
      m_message_window->error("No matches found yet");
      return true;
    }

    if (m_displayed_search_index - 1 < 0) {
      m_message_window->error("No previous matches");
      return true;
    }

    m_displayed_search_index--;
    m_extractor->move_to(m_search_result->get_match(m_displayed_search_index));
    return true;
  }
};

class SynchroniseLoop {
public:
  SynchroniseLoop(std::shared_ptr<FileEditor> file_editor, int delay)
      : m_file_editor(std::move(file_editor)), m_delay(delay) {}

  void loop() {
    while (true) {
      m_file_editor->synchronise();
      ftxui::ScreenInteractive* screen = ftxui::ScreenInteractive::Active();
      if (screen != nullptr) {
        screen->PostEvent(ftxui::Event::Custom);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(m_delay));
    }
  }

private:
  std::shared_ptr<FileEditor> m_file_editor;
  int m_delay;
};

void run_app(std::string fpath) {
  using namespace ftxui;

  auto extractor = std::make_shared<EditWindowExtractor>(fpath);

  auto background_task_message_window = std::make_shared<BackgroundTaskMessageWindow>();

  auto edit_window = std::make_shared<EditWindow>(extractor);

  auto runner_ptr = std::make_shared<BackgroundTaskRunner>();

  auto file_editor = std::make_shared<FileEditor>(edit_window, extractor,
                                                  background_task_message_window, runner_ptr);

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  auto loop = SynchroniseLoop(file_editor, DEFAULT_SYNCHRONISATION_DELAY);

  // Start the synchronise thread and the background thread
  auto synchronise_thread = std::thread([&loop] { loop.loop(); });

  auto background_thread = std::thread([&runner_ptr] { runner_ptr->loop(); });

  // Start the ftxui loop
  screen.Loop(file_editor);

  // Wait for the children threads
  synchronise_thread.join();

  background_thread.join();
}