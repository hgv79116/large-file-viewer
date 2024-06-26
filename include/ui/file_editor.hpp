#include "command_window.hpp"
#include "message_window.hpp"
#include "bg_log_window.hpp"
#include "edit_window.hpp"
#include "dispatchers/search_dispatch.hpp"
#include "util/safe_arg.hpp"
#include "util/file_stream.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <cxxopts.hpp>

#include <string>
#include <memory>

constexpr int32_t kDefaultMatchLimit = 5'000'000;

enum class Mode { View, Command };

class FileEditor : public ftxui::ComponentBase {
public:
  FileEditor(std::shared_ptr<EditWindow> edit_window,
             std::shared_ptr<EditWindowExtractor> extractor,
             std::shared_ptr<BgLogWindow> task_message_window,
             std::unique_ptr<SearchDispatcher> disp_ptr,
             FileMetadata fmeta)
      : _edit_window(std::move(edit_window)),
        _task_message_window(std::move(task_message_window)),
        _command_window(std::make_shared<CommandWindow>(
            [this](std::string command) { executeCommand(command); })),
        _message_window(std::make_shared<MessageWindow>()),

        _extractor(std::move(extractor)),

        _disp_ptr(std::move(disp_ptr)),

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
    if (_disp_ptr->getCurrent()) {
      if (handleSearchEvents(event)) {
        return true;
      }
    }

    // Handle unspecial events
    if (_message_window->isLockingScreen()) {
      return _message_window->OnEvent(event);
    }

    if (event == Event::Escape) {
      if (_mode != Mode::View) {
        switchMode(Mode::View);
        return true;
      }
    } else if (event == Event::Character('/')) {
      if (_mode != Mode::Command) {
        switchMode(Mode::Command);
        return true;
      }
    }

    if (_mode == Mode::View) {
      return _edit_window->OnEvent(event);
    }
    if (_mode == Mode::Command) {
      return _command_window->OnEvent(event);
    }

    // Should not happen
    exit(-1);

    return false;
  }

  ftxui::Element Render() override {
    if (_mode == Mode::View) {
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
    auto current_search = _disp_ptr->getCurrent();
    if (current_search) {
      BackgroundTaskStatus status = current_search->getStatus();
      switch (status) {
        case BackgroundTaskStatus::NotStarted:
          _task_message_window->setMessage("Search pending");
          break;
        case BackgroundTaskStatus::Running:
          _task_message_window->setMessage(
              "Searched until location " + std::to_string(current_search->getSearchProgress())
              + "... " + std::to_string(current_search->getNumMatch()) + " occurences found.");
          break;
        case BackgroundTaskStatus::Finished:
          _task_message_window->setMessage("Searche completed. "
                                             + std::to_string(current_search->getNumMatch())
                                             + " occurences found.");
          break;
        case BackgroundTaskStatus::Cancelled:
          _task_message_window->setMessage("Search canceled!");
          break;
      }
    }
  }

private:
  Mode _mode = Mode::View;
  std::shared_ptr<EditWindow> _edit_window;
  std::shared_ptr<BgLogWindow> _task_message_window;
  std::shared_ptr<CommandWindow> _command_window;
  std::shared_ptr<MessageWindow> _message_window;
  std::shared_ptr<EditWindowExtractor> _extractor;
  std::unique_ptr<SearchDispatcher> _disp_ptr;

  FileMetadata _fmeta;

  // Users' commands parsers
  cxxopts::Options _jump_options;
  cxxopts::Options _search_options;

  std::optional<Search::ConstResultIterator> _res_iter;

  void switchMode(Mode new_mode) {
    clearCurrentMode();
    setMode(new_mode);
  }

  void clearCurrentMode() {
    if (_mode == Mode::Command) {
      _command_window->clear();
    }
    _message_window->clear();
  }

  void setMode(Mode mode) { _mode = mode; }

  // static constexpr std::string
  void executeCommand(std::string command) {
    try {
      executeCommandUnguarded(std::move(command));
    } catch (cxxopts::OptionException const& e) {
      _message_window->error(e.what());
      return;
    }
  }

  void executeCommandUnguarded(const std::string& command) {
    SafeArg safe_arg(command);
    std::string command_type(safe_arg.get_argv()[0]);

    if (command_type == "exit") {
      exit(0);
    }

    if (command_type == "jump") {
      executeJumpCommand(safe_arg);
      return;
    }

    if (command_type == "search") {
      executeSearchCommand(safe_arg);
      return;
    }

    if (command_type == "cancel") {
      // Request search to cancel. The thread running this search won't really be stopped until
      // it reads the signal.
      _disp_ptr->requestCancelCurrentSearch();
      return;
    }

    _message_window->error("No such command: " + command);
  }

  void executeJumpCommand(const SafeArg& safe_arg) {
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

  void executeSearchCommand(const SafeArg& safe_arg) {
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

    {
      bool executed = _disp_ptr->tryDispatch(
          initialiseFstream(_fmeta.getPath()),
          {.pattern{pattern}, .start{from}, .end{to}, .limit = kDefaultMatchLimit});

      if (!executed) {
        _message_window->error("Cannot run searh. Probably another search is already running.");
      }
      else {
        // Reset UI variables
        resetSearchDisplay();
      }
    }
  }

  void resetSearchDisplay() {
    // Reset search variables
    _res_iter = _disp_ptr->getCurrent()->cbegin();
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
    int num_match = _disp_ptr->getCurrent()->getNumMatch();
    BackgroundTaskStatus status = _disp_ptr->getCurrent()->getStatus();
    bool search_finished_or_aborted
        = status == BackgroundTaskStatus::Finished || status == BackgroundTaskStatus::Cancelled;

    if (num_match == 0) {
      _message_window->error("No matches found yet");
      return true;
    }

    if (_res_iter.value() + 1 == _disp_ptr->getCurrent()->cend()) {
      if (!search_finished_or_aborted) {
        _message_window->error("Next match not found yet");
        return true;
      }

      _message_window->error("No more matches found");
      return true;
    }

    _res_iter = _res_iter.value() + 1;
    _extractor->move_to(*_res_iter.value());
    return true;
  }

  bool handleSearchReverseTabEvent() {
    int num_match = _disp_ptr->getCurrent()->getNumMatch();

    if (num_match == 0) {
      _message_window->error("No matches found yet");
      return true;
    }

    if (_res_iter == _disp_ptr->getCurrent()->cbegin()) {
      _message_window->error("No previous matches");
      return true;
    }

    _res_iter = _res_iter.value() - 1;
    _extractor->move_to(*_res_iter.value());
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