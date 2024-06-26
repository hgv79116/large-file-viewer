#include <LFV/app.hpp>
#include <LFV/background_task_runner.hpp>
#include <LFV/file_extractor.hpp>
#include <LFV/safe_arg.hpp>
#include <LFV/search_stream.hpp>
#include <LFV/file_stream.hpp>

#include <cstdint>
#include <cxxopts.hpp>
#include <deque>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <memory>
#include <string>
#include <vector>

constexpr int32_t kDefaultMatchLimit = 5'000'000;
constexpr int32_t kDefaultSyncDelay = 30;

class BgLogWindow final : public ftxui::ComponentBase {
public:
  ftxui::Element Render() override {
    return ftxui::paragraph(_message) | ftxui::color(ftxui::Color::SkyBlue1) | ftxui::bold;
  }

  bool OnEvent([[maybe_unused]] ftxui::Event event) override {
    // Do nothing.
    return false;
  };

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  [[nodiscard]] bool Focusable() const override { return true; }

  // APIs for state mutation by parent components and
  // updaters/synchronisers
  // Not thread-safe. Requires updaters/synchronisers to run on the same thread
  void set_message(std::string message) { _message = message; }

  void clear() { _message.clear(); }

private:
  std::string _message;
};

class MessageWindow final : public ftxui::ComponentBase {
public:
  ftxui::Element Render() override {
    using namespace ftxui;
    return get_formatted_message();
  }

  bool OnEvent(ftxui::Event event) override {
    // Release lock if locking the screen and the event match
    if (_required_response && _required_response == event) {
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

  bool is_locking_screen() { return _required_response.has_value(); }

private:
  enum class MessageType { INFO, WARNING, ERROR };
  struct Message {
    std::string content;
    MessageType type;
  };

  std::optional<Message> _displayed_message;

  std::optional<ftxui::Event> _required_response;

  ftxui::Element get_formatted_message() {
    using namespace ftxui;
    if (!_displayed_message) {
      return text("");
    }

    if (_displayed_message->type == MessageType::ERROR) {
      return paragraph("Error: " + _displayed_message->content + ". Press Escape to continue.")
             | color(Color::RedLight) | bold;
    }

    if (_displayed_message->type == MessageType::WARNING) {
      return paragraph("Warning: " + _displayed_message->content + ". Press Escape to continue.")
             | color(Color::Yellow) | bold;
    }

    // Info
    return paragraph(_displayed_message->content) | color(Color::LightCoral) | bold;
  }
};

class CommandWindow final : public ftxui::ComponentBase {
public:
  CommandWindow(std::function<void(std::string)> execute) : _execute(std::move(execute)) {
    init_input_field();
  }

  bool OnEvent(ftxui::Event event) override { return _input->OnEvent(event); }

  ftxui::Element Render() override {
    using namespace ftxui;
    return hbox({text("/"), _input->Render()})
           | size(WidthOrHeight::HEIGHT, Constraint::GREATER_THAN, 2);
  }

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  bool Focusable() const override { return true; }

  void clear() {
    _input->Detach();

    _current_command.clear();

    init_input_field();
  }

private:
  std::string _current_command;
  ftxui::Component _input;
  std::function<void(std::string)> _execute;

  void init_input_field() {
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

class EditWindow final : public ftxui::ComponentBase {
public:
  EditWindow(std::shared_ptr<EditWindowExtractor> extractor, FileMetadata fmeta)
    : _extractor(std::move(extractor)),
      _fmeta{fmeta}
    {}

  ftxui::Element Render() override {
    using namespace ftxui;

    // Adjust size if needed
    adjust_size();

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
    adjust_size();

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

  void adjust_size() {
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

class FileEditor : public ftxui::ComponentBase {
public:
  FileEditor(std::shared_ptr<EditWindow> edit_window,
             std::shared_ptr<EditWindowExtractor> extractor,
             std::shared_ptr<BgLogWindow> task_message_window,
             std::shared_ptr<BackgroundTaskRunner> runner_ptr,
             FileMetadata fmeta)
      : _edit_window(std::move(edit_window)),
        _task_message_window(std::move(task_message_window)),
        _command_window(std::make_shared<CommandWindow>(
            [this](std::string command) { execute_command(command); })),
        _message_window(std::make_shared<MessageWindow>()),

        _extractor(std::move(extractor)),

        _runner_ptr(std::move(runner_ptr)),
        
        _fmeta{fmeta},

        _jump_options("jump", "Jump to a location if the file"),
        _search_options("search", "Search a pattern") {
    _jump_options.add_options()("p,position", "Position to jump to in bytes",
                                 cxxopts::value<long long>());
    _jump_options.parse_positional({"position"});

    _search_options.add_options()("p,pattern", "Pattern to search", cxxopts::value<std::string>())(
        "f,from", "Starting position in bytes", cxxopts::value<long long>()->default_value("0"))(
        "t,to", "Ending position in bytes",
        cxxopts::value<long long>()->default_value(std::to_string(_fmeta.getSize())));
    _search_options.parse_positional({"pattern"});

    Add(_edit_window);
    Add(_task_message_window);
    Add(_message_window);
    Add(_command_window);
  }

  bool OnEvent(ftxui::Event event) override {
    using namespace ftxui;
    // Handle special events
    if (event == Event::Special({0})) {
      _edit_window->OnEvent(event);
      _message_window->OnEvent(event);
      _command_window->OnEvent(event);
      return true;
    }

    // Handle search events, if searching
    if (_search_result != nullptr) {
      if (handleSearchEvents(event)) {
        return true;
      }
    }

    // Handle unspecial events
    if (_message_window->is_locking_screen()) {
      return _message_window->OnEvent(event);
    }

    if (event == Event::Escape) {
      if (_mode != Mode::VIEW) {
        switch_mode(Mode::VIEW);
        return true;
      }
    } else if (event == Event::Character('/')) {
      if (_mode != Mode::COMMAND) {
        switch_mode(Mode::COMMAND);
        return true;
      }
    }

    if (_mode == Mode::VIEW) {
      return _edit_window->OnEvent(event);
    }
    if (_mode == Mode::COMMAND) {
      return _command_window->OnEvent(event);
    }

    // Should not happen
    exit(-1);

    return false;
  }

  ftxui::Element Render() override {
    if (_mode == Mode::VIEW) {
      // View mode
      return ftxui::vbox({_edit_window->Render(), _task_message_window->Render()});
    }

    // Command mode
    return ftxui::vbox({
        _edit_window->Render(),
        _task_message_window->Render(),
        _message_window->Render(),
        _command_window->Render(),
    });
  }

  void OnAnimation([[maybe_unused]] ftxui::animation::Params& params) override {
    // Do nothing
  }

  bool Focusable() const override { return true; }

  void synchronise() {
    // Synchronise UI state with background task if needed
    if (_search_result != nullptr) {
      BackgroundTaskStatus status = _search_result->get_status();
      switch (status) {
        case BackgroundTaskStatus::NOT_STARTED:
          _task_message_window->set_message("Search pending");
          break;
        case BackgroundTaskStatus::ONGOING:
          _task_message_window->set_message(
              "Searched until location " + std::to_string(_search_result->get_current_pos())
              + "... " + std::to_string(_search_result->get_num_matches()) + " occurences found.");
          break;
        case BackgroundTaskStatus::FINISHED:
          _task_message_window->set_message("Searche completed. "
                                             + std::to_string(_search_result->get_num_matches())
                                             + " occurences found.");
          break;
        case BackgroundTaskStatus::ABORTED:
          _task_message_window->set_message("Search canceled!");
          break;
      }
    }
  }

private:
  Mode _mode = Mode::VIEW;
  std::shared_ptr<EditWindow> _edit_window;
  std::shared_ptr<BgLogWindow> _task_message_window;
  std::shared_ptr<CommandWindow> _command_window;
  std::shared_ptr<MessageWindow> _message_window;
  std::shared_ptr<EditWindowExtractor> _extractor;
  std::shared_ptr<BackgroundTaskRunner> _runner_ptr;

  FileMetadata _fmeta;

  // Users' commands parsers
  cxxopts::Options _jump_options;
  cxxopts::Options _search_options;

  // Search result, if there is any
  static constexpr int NOT_DISPLAYED = -1;
  int _displayed_search_index = NOT_DISPLAYED;
  std::shared_ptr<SearchResult> _search_result;
  std::shared_ptr<std::atomic<bool>> _search_aborted;

  void switch_mode(Mode new_mode) {
    clear_current_mode();
    set_mode(new_mode);
  }

  void clear_current_mode() {
    if (_mode == Mode::COMMAND) {
      _command_window->clear();
    }
    _message_window->clear();
  }

  void set_mode(Mode mode) { _mode = mode; }

  // static constexpr std::string
  void execute_command(std::string command) {
    try {
      execute_command_unguarded(std::move(command));
    } catch (cxxopts::OptionException const& e) {
      _message_window->error(e.what());
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
      *_search_aborted = true;
      return;
    }

    _message_window->error("No such command: " + command);
  }

  void execute_jump_command(const SafeArg& safe_arg) {
    cxxopts::ParseResult parse_result
        = _jump_options.parse(safe_arg.get_argc(), safe_arg.get_argv());

    auto pos = static_cast<std::streampos>(parse_result["position"].as<long long>());

    if (pos < 0 || pos >= _extractor->getStreamEnd()) {
      _message_window->error("Invalid position: " + std::to_string(pos));
    } else {
      _extractor->move_to(pos);
      _message_window->info("Jumped to " + std::to_string(pos));
    }
  }

  void execute_search_command(const SafeArg& safe_arg) {
    cxxopts::ParseResult parse_result
        = _search_options.parse(safe_arg.get_argc(), safe_arg.get_argv());

    auto pattern = parse_result["pattern"].as<std::string>();
    auto from = static_cast<std::streampos>(parse_result["from"].as<long long>());
    auto to = static_cast<std::streampos>(parse_result["to"].as<long long>());

    if (pattern.empty()) {
      _message_window->error("Pattern cannot be empty");
      return;
    }

    if (from > to || from < 0 || from > _extractor->getStreamEnd() || to < 0
        || to > _extractor->getStreamEnd()) {
      _message_window->error("Invalid range: " + std::to_string(from) + " - "
                              + std::to_string(to));
      return;
    }

    if (!_runner_ptr->can_run_task()) {
      _message_window->error("Already running a background task. ");
      return;
    }

    // Reset search variables
    reset_search();

    // Capturing by reference leads to reading trash values when
    // the lambda is called later.
    _runner_ptr->run_task([this, pattern, from, to] {
      search_in_stream(std::ifstream(_fmeta.getPath()), pattern, from, to,
                       kDefaultMatchLimit, _search_result, _search_aborted);
    });
  }

  void reset_search() {
    // Reset search variables
    _displayed_search_index = NOT_DISPLAYED;
    _search_result = std::make_shared<SearchResult>();
    _search_aborted = std::make_shared<std::atomic<bool>>(false);
  }

  bool handleSearchEvents(ftxui::Event event) {
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

  bool handleSearchTabEvent() {
    int nu_match = _search_result->get_num_matches();
    BackgroundTaskStatus status = _search_result->get_status();
    bool search_finished_or_aborted
        = status == BackgroundTaskStatus::FINISHED || status == BackgroundTaskStatus::ABORTED;

    if (nu_match == 0) {
      _message_window->error("No matches found yet");
      return true;
    }

    if (_displayed_search_index + 1 >= nu_match) {
      if (!search_finished_or_aborted) {
        _message_window->error("Next match not found yet");
        return true;
      }

      _message_window->error("No more matches found");
      return true;
    }

    _displayed_search_index++;
    _extractor->move_to(_search_result->get_match(_displayed_search_index));
    return true;
  }

  bool handleSearchReverseTabEvent() {
    int nu_match = _search_result->get_num_matches();

    if (nu_match == 0) {
      _message_window->error("No matches found yet");
      return true;
    }

    if (_displayed_search_index - 1 < 0) {
      _message_window->error("No previous matches");
      return true;
    }

    _displayed_search_index--;
    _extractor->move_to(_search_result->get_match(_displayed_search_index));
    return true;
  }
};

class SynchroniseLoop {
public:
  SynchroniseLoop(std::shared_ptr<FileEditor> file_editor, int delay)
      : _file_editor(std::move(file_editor)), _delay(delay) {}

  void loop() {
    while (true) {
      _file_editor->synchronise();
      ftxui::ScreenInteractive* screen = ftxui::ScreenInteractive::Active();
      if (screen != nullptr) {
        screen->PostEvent(ftxui::Event::Custom);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(_delay));
    }
  }

private:
  std::shared_ptr<FileEditor> _file_editor;
  int _delay;
};

void run_app(std::string fpath) {
  using namespace ftxui;

  auto fmeta = FileMetadata(fpath);
  std::unique_ptr<std::ifstream> fstream = initialiseFstream(fpath);

  auto extractor = std::make_shared<EditWindowExtractor>(std::move(fstream), fmeta);

  auto background_task_message_window = std::make_shared<BgLogWindow>();

  auto edit_window = std::make_shared<EditWindow>(extractor, fmeta);

  auto runner_ptr = std::make_shared<BackgroundTaskRunner>();

  auto file_editor = std::make_shared<FileEditor>(edit_window, extractor,
                                                  background_task_message_window, runner_ptr, fmeta);

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  auto loop = SynchroniseLoop(file_editor, kDefaultSyncDelay);

  // Start the synchronise thread and the background thread
  auto synchronise_thread = std::thread([&loop] { loop.loop(); });

  auto background_thread = std::thread([&runner_ptr] { runner_ptr->loop(); });

  // Start the ftxui loop
  screen.Loop(file_editor);

  // Wait for the children threads
  synchronise_thread.join();

  background_thread.join();
}