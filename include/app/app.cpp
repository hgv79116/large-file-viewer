#include "app.hpp"

#include "dispatchers/search_dispatch.hpp"
#include "dispatchers/reverse_dispatch.hpp"
#include "search_engine/search.hpp"
#include "util/file_metadata.hpp"
#include "util/file_stream.hpp"
#include "ui/edit_window_extractor.hpp"
#include "ui/file_editor.hpp"

#include <cstdint>
#include <deque>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>

constexpr int32_t kDefaultSyncDelay = 30;

void run_app(std::string fpath) {
  using namespace ftxui;

  auto fmeta = FileMetadata(fpath);
  std::unique_ptr<std::ifstream> fstream = initialiseFstream(fpath);

  auto extractor = std::make_shared<EditWindowExtractor>(std::move(fstream), fmeta);

  auto background_task_message_window = std::make_shared<BgLogWindow>();

  auto edit_window = std::make_shared<EditWindow>(extractor, fmeta);

  auto file_editor = std::make_shared<FileEditor>(edit_window, extractor,
                                                  background_task_message_window,
                                                  std::make_unique<SearchDispatcher>(),
                                                  fmeta);

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  auto loop = SynchroniseLoop(file_editor, kDefaultSyncDelay);

  // Start the synchronise thread and the background thread
  auto synchronise_thread = std::thread([&loop] { loop.loop(); });

  // Start the ftxui loop
  screen.Loop(file_editor);

  // Wait for the children threads
  synchronise_thread.join();
}