#include "search_engine/search.hpp"

#include <mutex>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

// This class acts a simple PC-queue between the search engine and the UI
class ReverseDispatcher: public SearchListener {
public:
  void onEvent(SearchEvent event) override {
    _event_queue.emplace_back(event);

    _screen_ptr->PostEvent(ftxui::Event::Custom);
  }

  bool hasQueuedEvents() {
    std::unique_lock lock{_q_mutex};
    return !_event_queue.empty();
  }

  std::vector<SearchEvent> getQueuedEvents() {
    std::unique_lock lock{_q_mutex};

    auto ret = _event_queue;
    _event_queue.clear();

    return ret;
  }

private:
    std::vector<SearchEvent> _event_queue;
    std::mutex _q_mutex;
    ftxui::ScreenInteractive* _screen_ptr;
};