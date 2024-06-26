#include "search_dispatch.hpp"

bool SearchDispatcher::tryDispatch(std::unique_ptr<std::istream> stream_ptr, SearchConfig config) {
    // The check if a search can be dispatched and the dispatch needs to be tied (atomically)
    // together, therefore we put them together into a method.
    // Might change to std::expected later for more meaningful error messages
    std::unique_lock lock{_mutex};

    // Clear the current search
    if (_search_ptr && _search_ptr->ended()) {
      if (_th.joinable()) {
        _th.join();
      }

      _search_ptr.reset();
    }

    if (_search_ptr) {
        return false;
    }

    _search_ptr = std::make_unique<Search>(std::move(stream_ptr), config, std::make_unique<TaskLogger>());

    _th = std::thread([this] { _search_ptr->start(); });

    return true;
}

void SearchDispatcher::requestCancelCurrentSearch() {
    std::unique_lock lock{_mutex};

    if (_search_ptr) {
      _search_ptr->requestCancel();
    }
}