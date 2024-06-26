#pragma once

#include "background_task/task_logger.hpp"
#include "background_task/background_task.hpp"
#include "LFV/lfv_exception.hpp"

#include <memory>
#include <istream>
#include <string>
#include <chrono>

struct SearchConfig {
    std::string pattern;
    std::streampos start {0};
    // std::numeric_limits<std::streampos>::max() is not a thing
    // -1 works on mac, but is not portable else where
    // TODO: fix this
    // Since we use -1, it is thus unsafe to use < and > to check for break condition. Use equality
    // comparison instead.
    std::streampos end {-1};
    size_t limit {std::numeric_limits<size_t>::max()};
};

enum class SearchEventType { ProgressUpdate, FoundNew, Finished, Cancelled };

struct SearchEvent {
  SearchEventType type;
  std::chrono::time_point<std::chrono::system_clock> time_point;
};

class SearchListener {
public:
  // Listener should not access Search synchronously, otherwise deadlock
  // might happen.
  virtual void onEvent(SearchEvent event);
  ~SearchListener() = default;
};

// Thread safe class to manage a search.
// TODO: Add pub-sub; Add thread safety to iterators
class Search {
public:
  Search(std::unique_ptr<std::istream> stream_ptr, SearchConfig config, std::unique_ptr<TaskLogger> logger_ptr)
    : _stream_ptr {std::move(stream_ptr)},
      _config {config},
      _logger_ptr {std::move(logger_ptr)},
      _status {BackgroundTaskStatus::NotStarted},
      _cancel_requested{false},
      _search_progress{0} {
    // Using istream instead of fstream allows interface compatibility with
    // other types of streams. This allows easier testing as we can use stringtream

    if (config.pattern.empty()) {
          // Too important to be left to use, as empty search keys may
          // crash the application. At the same time, not too performance-critical.
          throw LFVException{"Search key cannot be empty"};
      }
  }

  using ResultIterator = std::vector<std::streampos>::iterator;

  ResultIterator begin() { return _matches.begin();  }

  ResultIterator end() { return _matches.end();  }

  // Launch the search
  void start();
  // Returns true if either canceled or finished
  bool ended() const {
    return cancelled() || finished();
  }
  // Returns true if the search has been canceled. Note that there might
  // be a delay between the cancel request and the cancellation.
  bool cancelled() const { return _status == BackgroundTaskStatus::Cancelled; }
  // Returns true if the search finished searching the whole file
  bool finished() const { return _status == BackgroundTaskStatus::Finished; }
  // Request for the cancellation of the search. There might be a delay between
  BackgroundTaskStatus getStatus() const { return _status; }
  // the cancel request and the cancellation.
  void requestCancel() {
    _cancel_requested = true;
  }
  // Returns the current number of matches. NOT THREAD SAFE
  int getNumMatch() const { return _matches.size();  }

  std::vector<std::streampos> getMatches() const { return _matches; }

  std::streampos getSearchProgress() const { return _search_progress; }

private:
  std::unique_ptr<std::istream> _stream_ptr;
  SearchConfig _config;
  std::unique_ptr<TaskLogger> _logger_ptr;
  std::atomic<BackgroundTaskStatus> _status;
  std::atomic<bool> _cancel_requested;
  std::atomic<std::streampos> _search_progress;
  // TODO: change to a different data structure handle larger number of matches
  std::vector<std::streampos> _matches;
  std::vector<SearchListener*> _listener_ptrs;

  using clock = std::chrono::system_clock;

  void addMatch(std::streampos pos);

  void setSearchProgress(std::streampos cur_pos, bool should_announce);

  void cancel();

  void announce(SearchEvent event);
};