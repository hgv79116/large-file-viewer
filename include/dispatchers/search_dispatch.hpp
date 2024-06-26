#pragma once

#include "search_engine/search.hpp"

#include <memory>
#include <mutex>

// This class is used to manage and dispatch searches.
// Currently the only policy is to allow only one search at a time.
class SearchDispatcher {
public:
    // Atomically dispatches the search.
    // Returns true if the search has been dispatched, false otherwise.
  bool tryDispatch(std::unique_ptr<std::istream> file_stream, SearchConfig config);

  void requestCancelCurrentSearch();
private:
  std::mutex _mutex;
  std::unique_ptr<Search> _search_ptr;
};